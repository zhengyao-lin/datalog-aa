// An intermediate representation for
// datalog. It's used for generating backend
// code (for specific solvers) and
// potential in-house optimization

#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <iostream>

/**
 * This is a sorted subset of the language bddbddb supports
 * 
 * syntax
 * 
 * term := constant
 *       | variable
 * 
 * atom := R(term [, term]*) where R is a relation symbol
 * 
 * formula := atom
 *          | atom <- atom [, atom]*
 * 
 * program := (formula .)+
 */

/**
 * Require that S and C are different
 */
template<typename S /* symbol type */, typename C /* constant type */>
class Datalog {
public:
    class Sort;
    class Term;
    class Formula;

    using SymbolVector = std::vector<S>;
    using TermVector = std::vector<Term>;
    using FormulaVector = std::vector<Formula>;

    /**
     * A term is either a constant or a variable
     */
    class Term {
        S var;
        C value;
        bool is_var;

    public:
        Term(const S &var): var(var), is_var(true) {}
        Term(const C &value): value(value), is_var(false) {}

        bool isVariable() const { return is_var; }
        const S &getVariable() const { return var; }
        const C &getValue() const { return value; }
    };

    /**
     * A formula is either a atomic proposition or
     * a horn clause of atomic propositions
     */
    class Formula {
        S relation_name; // relation symbol
        TermVector args;
        FormulaVector body;

    public:
        Formula(const S &relation_name, const TermVector &args):
            relation_name(relation_name), args(args) {}

        Formula(const Formula &head, const FormulaVector &body):
            relation_name(head.getRelationName()), args(head.getArguments()), body(body) {
            assert(head.isAtom() && "cannot use a horn clause as head");
        }

        Formula(const S &relation_name, const TermVector &args, const FormulaVector &body):
            relation_name(relation_name), args(args), body(body) {
            for (auto const &sub_term: body) {
                assert(sub_term.isAtom() && "subterm of a horn clause but be an atom");
            }
        }

        // e.g. Formula("relation_name", 1, 2, "this_is_a_variable")
        template<typename ...Ts>
        Formula(const S &relation_name, Ts ...args):
            relation_name(relation_name), args(parseTermVector<Ts...>(args...)) {}

        const S &getRelationName() const { return relation_name; }
        unsigned int getArity() const { return args.size(); }

        const Term &getArgument(unsigned int i) const {
            assert(i < args.size() && "index out of range");
            return args.at(i);
        }

        bool isAtom() const { return body.empty(); }

        /**
         * A reversed implication, e.g.
         * Formula("rel1", 2, 4).given(
         *     Formula("rel2", 2, 3),
         *     Formula("rel2", 3, 4)
         * )
         */
        template<typename ...Ts>
        Formula given(Ts ...args) {
            return Formula(*this, parseFormulaVector<Ts...>(args...));
        }

        const TermVector &getArguments() const { return args; }
        const FormulaVector &getBody() const { return body; }
    };

    class Sort {
        static const unsigned int DEFAULT_SIZE = 65535;

        S name;
        unsigned int size;

    public:
        Sort(const S &name, unsigned int size): name(name), size(size) {}
        Sort(const S &name): name(name), size(DEFAULT_SIZE) {}

        const S &getName() const { return name; }
        unsigned int getSize() const { return size; }
    };

    class Relation {
        S name;
        SymbolVector sort_names;

    public:
        Relation(const S &name, const SymbolVector &sort_names):
            name(name), sort_names(sort_names) {}

        Relation(const S &name): name(name) {}

        template<typename ...Ts>
        Relation(const S &name, Ts ...args):
            name(name), sort_names(parseSymbolVector<Ts...>(args...)) {}

        Formula getAtom(const TermVector &terms) const {
            return new Formula(name, terms);
        }

        const S &getName() const { return name; }

        const S &getArgumentSortName(unsigned int idx) const {
            assert(idx < sort_names.size() && "index out of range");
            return sort_names.at(idx);
        }

        const SymbolVector &getArgumentSortNames() const { return sort_names; }

        template<typename ...Ts>
        Formula operator()(Ts ...args) const {
            TermVector terms = parseTermVector<Ts...>(args...);
            assert(terms.size() == sort_names.size() &&
                   "number of terms does not match the number of sorts");

            return Formula(name, terms);
        }
    };

    class Program {
        std::map<S, Sort> sorts;
        std::map<S, Relation> relations;
        FormulaVector formulas;

    public:
        void addSort(const Sort &sort) {
            assert(!hasSort(sort.getName()) && "duplicated sort");
            sorts.insert(std::make_pair(sort.getName(), sort));
        }

        void addRelation(const Relation &relation) {
            assert(!hasRelation(relation.getName()) && "duplicated relation");
            relations.insert(std::make_pair(relation.getName(), relation));
        }

        void addFormula(const Formula &formula) {
            assert(hasRelation(formula.getRelationName()) &&
                   "formula added before the relation has been declared");
            formulas.push_back(formula);
        }

        const std::map<S, Sort> &getSorts() const { return sorts; }
        const std::map<S, Relation> &getRelations() const { return relations; }
        const FormulaVector &getFormulas() const { return formulas; }

        bool hasSort(const S &name) const {
            return sorts.find(name) != sorts.end();
        }

        bool hasRelation(const S &name) const {
            return relations.find(name) != relations.end();
        }

        const Relation &getRelation(const S &name) const {
            assert(hasRelation(name) && "relation does not exist");
            return relations.at(name);
        }

        // TODO: add well-formness check
        bool isWellFormed() const;
    };

    class Engine {
    public:
        virtual void load(const Program &program) = 0;
        virtual FormulaVector query(const S &relation_name) = 0;
    };

    /**
     * Utility functions for parsing variadic arugments of sorts, terms, and formulas
     */

    template<typename = void>
    static constexpr SymbolVector parseSymbolVector() {
        return SymbolVector();
    }

    template<typename = S, typename ...Rest>
    static constexpr SymbolVector parseSymbolVector(S first, Rest ...rest) {
        SymbolVector symbols = parseSymbolVector<Rest...>(rest...);
        symbols.insert(symbols.begin(), first);
        return symbols;
    }

    /**
     * Also supports passing a sort as a symbol
     */
    template<typename = Sort, typename ...Rest>
    static constexpr SymbolVector parseSymbolVector(Sort first, Rest ...rest) {
        SymbolVector symbols = parseSymbolVector<Rest...>(rest...);
        symbols.insert(symbols.begin(), first.getName());
        return symbols;
    }

    template<typename = void>
    static constexpr TermVector parseTermVector() {
        return TermVector();
    }

    template<typename = S, typename ...Rest>
    static constexpr TermVector parseTermVector(S first, Rest ...rest) {
        TermVector terms = parseTermVector<Rest...>(rest...);
        terms.insert(terms.begin(), Term(first));
        return terms;
    }

    template<typename = C, typename ...Rest>
    static constexpr TermVector parseTermVector(C first, Rest ...rest) {
        TermVector terms = parseTermVector<Rest...>(rest...);
        terms.insert(terms.begin(), Term(first));
        return terms;
    }

    template<typename = Term, typename ...Rest>
    static constexpr TermVector parseTermVector(Term first, Rest ...rest) {
        TermVector terms = parseTermVector<Rest...>(rest...);
        terms.insert(terms.begin(), first);
        return terms;
    }

    template<typename = void>
    static constexpr FormulaVector parseFormulaVector() {
        return FormulaVector();
    }

    template<typename = Formula, typename ...Rest>
    static constexpr FormulaVector parseFormulaVector(Formula first, Rest ...rest) {
        FormulaVector formulas = parseFormulaVector<Rest...>(rest...);
        formulas.insert(formulas.begin(), first);
        return formulas;
    }
};

using StandardDatalog = Datalog<std::string, unsigned int>;

/**
 * Prints out the program in the bddbddb syntax
 */
std::ostream &operator<<(std::ostream &out, const StandardDatalog::Term &term);
std::ostream &operator<<(std::ostream &out, const StandardDatalog::Formula &formula);
std::ostream &operator<<(std::ostream &out, const StandardDatalog::Sort &sort);
std::ostream &operator<<(std::ostream &out, const StandardDatalog::Relation &relation);
std::ostream &operator<<(std::ostream &out, const StandardDatalog::Program &program);