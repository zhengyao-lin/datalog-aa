#include "DatalogIR.h"

using namespace llvm;

raw_ostream &operator<<(raw_ostream &out, const StandardDatalog::Term &term) {
    if (term.isVariable()) {
        out << term.getVariable();
    } else {
        out << term.getValue();
    }

    return out;
}

raw_ostream &operator<<(raw_ostream &out, const StandardDatalog::Formula &formula) {
    out << formula.getRelationName() << "("; 
    bool first = true;

    for (auto const &arg: formula.getArguments()) {
        if (first) {
            first = false;
        } else {
            out << ", ";
        }

        out << arg;
    }

    out << ")";

    if (!formula.getBody().empty()) {
        out << " :- ";
        first = true;
        
        for (auto const &sub_term: formula.getBody()) {
            if (first) {
                first = false;
            } else {
                out << ", ";
            }

            out << sub_term;
        }
    }

    return out;
}

raw_ostream &operator<<(raw_ostream &out, const StandardDatalog::Sort &sort) {
    out << sort.getName() << " " << sort.getSize();
    return out;
}

raw_ostream &operator<<(raw_ostream &out, const StandardDatalog::Relation &relation) {
    out << relation.getName() << "(";
    bool first = true;
    unsigned int var_indx = 0;

    for (auto const &sort: relation.getArgumentSortNames()) {
        if (first) {
            first = false;
        } else {
            out << ", ";
        }

        out << "V" << var_indx++ << ": " << sort;
    }

    out << ") printtuples";

    return out;
}

raw_ostream &operator<<(raw_ostream &out, const StandardDatalog::Program &program) {
    for (auto const &item: program.getSorts()) {
        out << item.second << "\n";
    }

    out << "\n";

    for (auto const &item: program.getRelations()) {
        out << item.second << "\n";
    }

    out << "\n";

    for (auto const &formula: program.getFormulas()) {
        out << formula << ".\n";
    }

    return out;
}
