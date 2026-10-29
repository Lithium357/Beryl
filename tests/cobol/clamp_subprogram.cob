identification division.
program-id. CLAMPREC.
data division.
linkage section.
01 account-record.
    05 account-value pic 9(3).
procedure division using account-record.
    if account-value < 100
        add 1 to account-value
    else
        move 100 to account-value
    end-if
    move 7 to return-code
    goback.
