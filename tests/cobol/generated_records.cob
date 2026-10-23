identification division.
program-id. generated-records.
environment division.
input-output section.
file-control.
    select input-file assign to "INPUT.DAT"
        organization sequential file status input-status.
    select output-file assign to "OUTPUT.DAT"
        organization sequential.
data division.
file section.
fd input-file.
01 input-record.
    05 name-field pic x(4).
    05 amount-field pic s9(3)v9(2) comp-3.
    05 count-field pic 9(4).
    05 signed-count pic s9(2).
fd output-file.
01 output-record pic x(13).
working-storage section.
01 input-status pic xx.
procedure division.
    open input input-file
    open output output-file
    perform until input-status = "10"
        read input-file
        evaluate input-status
        when "00"
            if amount-field is not numeric or count-field is not numeric
                or signed-count is not numeric
                move 7 to return-code goback
            end-if
            move input-record to output-record
            write output-record
        when "10" continue
        when other move 8 to return-code goback
        end-evaluate
    end-perform
    close input-file output-file
    move 0 to return-code
    goback.
