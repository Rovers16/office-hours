/*  knowledge_base.pl - loads the generated facts and the rules.

    Build the facts first with `make`, then either
        swipl prolog/knowledge_base.pl          (ask questions directly)
        python3 ask.py ...                      (ask through pyswip)

    Paths are relative to this file, so it can be loaded from any directory.
*/

% Declared dynamic so a query still works (and simply fails) when a file has
% no facts of that kind, e.g. a faculty file where nobody supplied office hours.
:- dynamic course/5, meets/5,
           professor/4, teaches/2, office_hour/5,
           assigned/3, scheduled_office_hour/5, office_hours_source/2.

:- consult('../build/schedule').
:- consult('../build/faculty').
:- consult('../output/office_hours').
:- consult(rules).
