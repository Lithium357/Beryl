identification division.
program-id. codec-vectors.
environment division.
input-output section.
file-control.
    select input-file assign to "INPUT.DAT"
        organization sequential.
    select output-file assign to "OUTPUT.DAT"
        organization sequential.
data division.
file section.
fd input-file.
01 input-record pic x(25).
fd output-file.
01 output-record pic x(25).
working-storage section.
01 expected-record.
    05 label-field pic x(4).
    05 unsigned-display pic 9(3).
    05 signed-display pic s9(3)v9(2).
    05 signed-odd pic s9(3) comp-3.
    05 signed-even pic s9(4) comp-3.
    05 unsigned-odd pic 9(3) comp-3.
    05 unsigned-even pic 9(4) comp-3.
    05 scaled-packed pic s9(3)v9(2) comp-3.
01 case-number binary-long.
procedure division.
    open input input-file
    open output output-file
    perform varying case-number from 1 by 1 until case-number > 3
        evaluate case-number
        when 1
            move "AB" to label-field
            move 123 to unsigned-display
            move -12.34 to signed-display
            move -123 to signed-odd
            move 1234 to signed-even
            move 123 to unsigned-odd
            move 1234 to unsigned-even
            move -123.45 to scaled-packed
        when 2
            move spaces to label-field
            move 0 to unsigned-display signed-display signed-odd
                signed-even unsigned-odd unsigned-even scaled-packed
        when 3
            move "~~~~" to label-field
            move 999 to unsigned-display unsigned-odd
            move -999.99 to signed-display
            move 999 to signed-odd
            move -9999 to signed-even
            move 9999 to unsigned-even
            move 999.99 to scaled-packed
        end-evaluate
        read input-file
            at end move 8 to return-code goback
        end-read
        if input-record not = expected-record
            move 7 to return-code
            goback
        end-if
        move expected-record to output-record
        write output-record
    end-perform
    close input-file output-file
    move 0 to return-code
    goback.
