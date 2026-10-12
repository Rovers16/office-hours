/*  rules.pl - rules for answering questions about teaching and office hours.

    The facts these rules use are generated, never typed in by hand:

      build/schedule.pl   (syllabus_reader.py, from the syllabus)
          course(Code, Title, Section, Lecturer, Room).
          meets(Code, Section, Day, Start, End).
      build/faculty.pl    (syllabus_reader.py, from the faculty file)
          professor(Name, Department, Email, Office).
          teaches(Name, Code).
          office_hour(Name, Day, Start, End, Code).     % supplied by lecturers
      output/office_hours.pl   (the C program's results)
          assigned(Professor, Code, Section).
          scheduled_office_hour(Professor, Day, Start, End, Code).
          office_hours_source(Professor, generated | provided).

    Days are lower-case atoms (monday). Times are 'HH:MM' atoms ('09:30'); being
    zero-padded, they sort correctly with @< and @>=. Missing values are none.
*/

/* ---- days and times ---------------------------------------------------- */

day(monday). day(tuesday). day(wednesday). day(thursday). day(friday).
day(saturday). day(sunday).

weekday(D) :- day(D), D \== saturday, D \== sunday.

% day_index(?Day, ?N): Monday is 1, so answers can be sorted in week order.
day_index(Day, N) :- nth1(N, [monday, tuesday, wednesday, thursday, friday, saturday, sunday], Day).

% minutes(+Time, -Minutes): '13:30' -> 810.
minutes(Time, Minutes) :-
    Time \== none,
    atomic_list_concat([H, M], ':', Time),
    atom_number(H, HN), atom_number(M, MN),
    Minutes is HN * 60 + MN.

% Is Time inside [Start, End)?
during(Time, Start, End) :- Start \== none, Start @=< Time, Time @< End.

overlaps(S1, E1, S2, E2) :- S1 \== none, S2 \== none, S1 @< E2, S2 @< E1.

/* ---- finding things by a loosely typed name ---------------------------- */

% prof_named(+Text, -Professor): "mensah" finds 'Dr. Kwame Mensah'.
prof_named(Text, Prof) :-
    professor(Prof, _, _, _),
    sub_atom_icasechk(Prof, _, Text).

% course_named(+Text, -Code): "cosc369" and "COSC 369" both find 'COSC 369'.
course_named(Text, Code) :-
    code_key(Text, Key),
    distinct(Code, course(Code, _, _, _, _)),
    code_key(Code, Key).

code_key(Code, Key) :-
    upcase_atom(Code, Up),
    atom_chars(Up, Chars),
    include([C]>>char_type(C, alnum), Chars, Kept),
    atom_chars(Key, Kept).

/* ---- who teaches what, when and where ---------------------------------- */

% who_teaches(?Code, ?Professor)
who_teaches(Code, Prof) :- assigned(Prof, Code, _).
who_teaches(Code, Lecturer) :-              % named in the syllabus but not in the faculty file
    course(Code, _, Section, Lecturer, _),
    Lecturer \== none,
    \+ assigned(_, Code, Section).

% teaches_course(?Professor, ?Code, ?Title)
teaches_course(Prof, Code, Title) :-
    assigned(Prof, Code, Section),
    course(Code, Title, Section, _, _).

% class_time(?Professor, ?Code, ?Day, ?Start, ?End)
class_time(Prof, Code, Day, Start, End) :-
    assigned(Prof, Code, Section),
    meets(Code, Section, Day, Start, End).

% where_is(?Code, ?Room)
where_is(Code, Room) :- course(Code, _, _, _, Room).

% meets_on(?Day, ?Code, ?Start, ?End, ?Room): every class on a day.
meets_on(Day, Code, Start, End, Room) :-
    meets(Code, Section, Day, Start, End),
    course(Code, _, Section, _, Room).

online_course(Code) :-
    course(Code, _, Section, _, _),
    \+ ( meets(Code, Section, _, Start, _), Start \== none ).

evening_class(Code, Day, Start) :-
    meets(Code, _, Day, Start, _),
    Start \== none,
    Start @>= '17:00'.

department_of(Prof, Department) :- professor(Prof, Department, _, _).
contact(Prof, Email, Office) :- professor(Prof, _, Email, Office).

colleagues(A, B) :-
    professor(A, Department, _, _),
    professor(B, Department, _, _),
    A @< B.

/* ---- office hours ------------------------------------------------------ */

% office_hours_on(?Day, ?Professor, ?Start, ?End, ?Code)
office_hours_on(Day, Prof, Start, End, Code) :-
    scheduled_office_hour(Prof, Day, Start, End, Code).

% office_hours_for(?Code, ?Professor, ?Day, ?Start, ?End): help for one course.
office_hours_for(Code, Prof, Day, Start, End) :-
    scheduled_office_hour(Prof, Day, Start, End, Code).

provided_by_lecturer(Prof) :- office_hours_source(Prof, provided).
generated_for(Prof) :- office_hours_source(Prof, generated).

in_class(Prof, Day, Time) :-
    class_time(Prof, _, Day, Start, End),
    during(Time, Start, End).

in_office(Prof, Day, Time, Code) :-
    scheduled_office_hour(Prof, Day, Start, End, Code),
    during(Time, Start, End).

% available(?Professor, +Day, +Time): holding office hours and not in class.
available(Prof, Day, Time) :-
    in_office(Prof, Day, Time, _),
    \+ in_class(Prof, Day, Time).

% on_campus(?Professor, ?Day): teaching or holding office hours that day.
on_campus(Prof, Day) :-
    professor(Prof, _, _, _),
    day(Day),
    once(( class_time(Prof, _, Day, _, _)
         ; scheduled_office_hour(Prof, Day, _, _, _) )).

% free_day(?Professor, ?Day): a weekday with no classes and no office hours.
free_day(Prof, Day) :-
    professor(Prof, _, _, _),
    weekday(Day),
    \+ on_campus(Prof, Day).

% clash(?Professor, ?Day, ?Code, ?Start, ?End): an office hour overlapping a class.
% The C program never generates one; this catches lecturer-supplied ones.
clash(Prof, Day, Code, OStart, OEnd) :-
    scheduled_office_hour(Prof, Day, OStart, OEnd, _),
    class_time(Prof, Code, Day, CStart, CEnd),
    overlaps(OStart, OEnd, CStart, CEnd).

% weekly_office_minutes(?Professor, -Minutes)
weekly_office_minutes(Prof, Total) :-
    professor(Prof, _, _, _),
    aggregate_all(sum(M),
                  ( scheduled_office_hour(Prof, _, S, E, _),
                    minutes(S, SM), minutes(E, EM),
                    M is EM - SM ),
                  Total).

% course_without_office_hours(?Code): a taught course nobody holds office hours for.
course_without_office_hours(Code) :-
    distinct(Code, assigned(_, Code, _)),
    \+ scheduled_office_hour(_, _, _, _, Code).
