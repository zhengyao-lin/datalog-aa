#include "DatalogIR.h"

std::ostream &operator<<(std::ostream &out, const StandardDatalog::Term &term) {
    if (term.isVariable()) {
        out << term.getVariable();
    } else {
        out << term.getValue();
    }

    return out;
}

std::ostream &operator<<(std::ostream &out, const StandardDatalog::Formula &formula) {
    out << formula.getName() << "("; 
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

std::ostream &operator<<(std::ostream &out, const StandardDatalog::Sort &sort) {
    out << sort.getName() << " " << sort.getSize();
    return out;
}

std::ostream &operator<<(std::ostream &out, const StandardDatalog::Relation &relation) {
    out << relation.getName() << "(";
    bool first = true;

    for (auto const &sort: relation.getArgumentSorts()) {
        if (first) {
            first = false;
        } else {
            out << ", ";
        }

        out << sort;
    }

    out << ")";

    return out;
}

std::ostream &operator<<(std::ostream &out, const StandardDatalog::Program &program) {
    for (auto const &item: program.getSorts()) {
        out << item.second << std::endl;
    }

    out << std::endl;

    for (auto const &item: program.getRelations()) {
        out << item.second << std::endl;
    }

    out << std::endl;

    for (auto const &formula: program.getFormulas()) {
        out << formula << std::endl;
    }

    return out;
}
