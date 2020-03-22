/**
 * A macro-based DSL for writing datalog IR
 * 
 * Note: no variables/sort/relation starting with _ is allowed
 * 
 * example program:
    #include "DatalogDSL.h" // switch on the dsl
    StandardDatalog::Program program = BEGIN
        sort(V, 65535);

        rel(vertex, V);
        rel(edge, V, V);
        rel(path, V, V);
        var(x); var(y); var(z);

        path(x, x) <<= vertex(x);
        path(x, y) <<= edge(x, y);
        path(x, z) <<= path(x, y) & path(y, z);

        fact vertex(1);
        fact vertex(2);
        fact vertex(3);

        fact edge(1, 2);
        fact edge(2, 3);
    END;
    #include "DatalogDSL.h" // switch off the dsl
 */

#include "DatalogIR.h"

#ifndef _DATALOG_DSL_HELPER_
#define _DATALOG_DSL_HELPER_

struct DatalogDSLEnvironment {
    StandardDatalog::Program program;
    bool is_next_fact = false;
    unsigned int variable_counter = 0;

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

    std::string getFreshVariable() {
        return "_" + std::to_string(variable_counter++);
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

struct DatalogDSLWildcard {
    DatalogDSLEnvironment *env;
    DatalogDSLWildcard(DatalogDSLEnvironment &env): env(&env) {}

    operator std::string() {
        return env->getFreshVariable();
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
            DatalogDSLEnvironment _env; \
            DatalogDSLWildcard _(_env);

    #define VALID_NAME(name) assert(name[0] != '_' && "symbol cannot start with underscore")

    #define sort(name, size) \
        VALID_NAME(#name); \
        std::string name = #name; \
        _env.program.addSort(StandardDatalog::Sort(#name, size))

    #define rel(name, ...) \
        VALID_NAME(#name); \
        auto name = ([&] () { \
            StandardDatalog::Relation relation(#name, { __VA_ARGS__ }); \
            _env.program.addRelation(relation); \
            return DatalogDSLRelation(_env, #name); \
        })()

    #define var(name) \
        VALID_NAME(#name); \
        std::string name = #name

    #define fact _env.is_next_fact = true;

    #define END \
            return _env.program; \
        })()
#endif
