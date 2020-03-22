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

template<typename S /* symbol type */, typename C /* constant type */>
class Datalog {
public:
    class Term;
    class Formula;

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
        S symbol; // relation symbol
        TermVector args;
        FormulaVector body;

    public:
        Formula(const S &symbol, const TermVector &args):
            symbol(symbol), args(args) {}

        Formula(const Formula &head, const FormulaVector &body):
            symbol(head.getName()), args(head.getArguments()), body(body) {
            assert(head.isAtom() && "cannot use a horn clause as head");
        }

        Formula(const S &symbol, const TermVector &args, const FormulaVector &body):
            symbol(symbol), args(args), body(body) {
            for (auto const &sub_term: body) {
                assert(sub_term.isAtom() && "subterm of a horn clause but be an atom");
            }
        }

        const S &getName() const { return symbol; }
        unsigned int getArity() const { return args.size(); }

        const Term &getArgument(unsigned int i) const {
            assert(i < args.size() && "index out of range");
            return args.at(i);
        }

        bool isAtom() const { return body.empty(); }

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
        std::vector<S> sorts;

    public:
        Relation(const S &name, const std::vector<S> &sorts):
            name(name), sorts(sorts) {}

        Formula getAtom(const TermVector &terms) const {
            return new Formula(name, terms);
        }

        const S &getName() const { return name; }

        const S &getArgumentSort(unsigned int idx) const {
            assert(idx < sorts.size() && "index out of range");
            return sorts.at(idx);
        }

        const std::vector<S> &getArgumentSorts() const { return sorts; }
    };

    class Program {
        std::map<S, Sort> sorts;
        std::map<S, Relation> relations;
        FormulaVector formulas;

    public:
        void addSort(const Sort &sort) {
            assert(sorts.find(sort.getName()) == sorts.end() && "duplicated sort");
            sorts.insert(std::make_pair(sort.getName(), sort));
        }

        void addRelation(const Relation &relation) {
            assert(relations.find(relation.getName()) == relations.end() && "duplicated relation");
            relations.insert(std::make_pair(relation.getName(), relation));
        }

        void addFormula(const Formula &formula) { formulas.push_back(formula); }

        const std::map<S, Sort> &getSorts() const { return sorts; }
        const std::map<S, Relation> &getRelations() const { return relations; }
        const FormulaVector &getFormulas() const { return formulas; }

        const Relation &getRelation(const S &name) const {
            assert(relations.find(name) != relations.end() && "relation does not exist");
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