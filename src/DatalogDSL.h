/**
 * A macro-based DSL for writing datalog IR
 * 
 * example program:
    #include "DatalogDSL.h" // switch on the dsl
    StandardDatalog::Program program = BEGIN
        SORT(vertex, 10);
        REL(edge, vertex, vertex);
        REL(path, vertex, vertex);
        
        HORN(
            ATOM(path, string("x"), string("y")),
            ATOM(edge, string("x"), string("y"))
        );

        HORN(
            ATOM(path, string("x"), string("z")),
            ATOM(path, string("x"), string("y")),
            ATOM(path, string("y"), string("z"))
        );

        FACT(edge, 1, 2);
        FACT(edge, 2, 3);
        FACT(edge, 3, 4);
        FACT(edge, 2, 5);
        FACT(edge, 3, 6);
    END;
    #include "DatalogDSL.h" // switch off the dsl
 */

#include "DatalogIR.h"

#ifndef _DATALOG_DSL_HELPER_
#define _DATALOG_DSL_HELPER_

// cannot declare functions in block scope...
// so class it is ;)
struct DatalogDSLEnvironment {
    StandardDatalog::Program program;
    bool is_next_fact = false;

    template<typename = void>
    constexpr StandardDatalog::TermVector parseTermVector() {
        return StandardDatalog::TermVector();
    }

    template<typename = std::string, typename ...Rest>
    constexpr StandardDatalog::TermVector parseTermVector(std::string first, Rest ...rest) {
        StandardDatalog::TermVector terms = parseTermVector<Rest...>(rest...);
        terms.insert(terms.begin(), StandardDatalog::Term(first));
        return terms;
    }

    template<typename = unsigned int, typename ...Rest>
    constexpr StandardDatalog::TermVector parseTermVector(unsigned int first, Rest ...rest) {
        StandardDatalog::TermVector terms = parseTermVector<Rest...>(rest...);
        terms.insert(terms.begin(), StandardDatalog::Term(first));
        return terms;
    }
};

struct DatalogDSLAtom;
struct DatalogDSLHornBody {
    DatalogDSLEnvironment *env;
    StandardDatalog::FormulaVector formulas;

    DatalogDSLHornBody(DatalogDSLEnvironment &env): env(&env) {}

    inline void append(const StandardDatalog::Formula &formula) {
        formulas.push_back(formula);
    }

    inline DatalogDSLHornBody &operator&(const DatalogDSLAtom &other);
};

struct DatalogDSLAtom {
    DatalogDSLEnvironment *env;
    StandardDatalog::Formula atom;

    DatalogDSLAtom(DatalogDSLEnvironment &env, const StandardDatalog::Formula &atom):
        env(&env), atom(atom) {}

    inline DatalogDSLHornBody operator&(const DatalogDSLAtom &other) {
        DatalogDSLHornBody body(*env);
        body.append(atom);
        body.append(other.atom);
        return body;
    }
};

struct DatalogDSLRelation {
    DatalogDSLEnvironment *env;
    std::string name;

    DatalogDSLRelation(DatalogDSLEnvironment &env, const std::string &name):
        env(&env), name(name) {}

    template<typename ...Ts>
    inline DatalogDSLAtom operator()(Ts ...args) const {
        StandardDatalog::TermVector terms = env->parseTermVector<Ts...>(args...);
        // push the term into the current environment
        StandardDatalog::Formula atom(name, terms);

        if (env->is_next_fact) {
            env->program.addFormula(atom);
            env->is_next_fact = false;
        }

        return DatalogDSLAtom(*env, atom);
    }
};

inline DatalogDSLHornBody &DatalogDSLHornBody::operator&(const DatalogDSLAtom &other) {
    append(other.atom);
    return *this;
}

inline void operator<<=(const DatalogDSLAtom &head, const DatalogDSLHornBody &body) {
    body.env->program.addFormula(StandardDatalog::Formula(head.atom, body.formulas));
}

inline void operator<<=(const DatalogDSLAtom &head, const DatalogDSLAtom &body) {
    StandardDatalog::FormulaVector formulas;
    formulas.push_back(body.atom);
    body.env->program.addFormula(StandardDatalog::Formula(head.atom, formulas));
}

#endif // #ifndef _DATALOG_DSL_HELPER_

#ifdef IN_DSL
    #undef BEGIN
    #undef SORT
    #undef REL
    #undef ATOM
    #undef HORN
    #undef FACT
    #undef END
    #undef IN_DSL
#else
    #define IN_DSL

    #define BEGIN \
        ([] () -> StandardDatalog::Program { \
            DatalogDSLEnvironment env;

    #define sort(name, size) \
        std::string name = #name; \
        env.program.addSort(StandardDatalog::Sort(#name, size))

    #define rel(name, ...) \
        auto name = ([&] () { \
            StandardDatalog::Relation relation(#name, { __VA_ARGS__ }); \
            env.program.addRelation(relation); \
            return DatalogDSLRelation(env, #name); \
        })()

    #define var(name) std::string name = #name

    #define fact env.is_next_fact = true;

    #define END \
            return env.program; \
        })()
#endif
