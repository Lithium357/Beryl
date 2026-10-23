identification division.
program-id. accounts-bug.
environment division.
input-output section.
file-control.
    select input-file assign to "INPUT.DAT"
        organization is sequential.
    select output-file assign to "OUTPUT.DAT"
        organization is sequential.
data division.
file section.
fd input-file.
01 input-record pic 9(3).
fd output-file.
01 output-record pic 9(3).
procedure division.
    open input input-file
    open output output-file
    read input-file
        at end move 0 to input-record
    end-read
    if input-record > 100
        move 100 to output-record
    else
        compute output-record = input-record + 1
    end-if
    write output-record
    close input-file output-file
    stop run.
