*> Free-format example: 13 bytes per fixed-length record.
01 ACCOUNT-RECORD.
    05 ACCOUNT-ID PIC X(4).
    05 AMOUNT PIC S9(5)V9(2) COMP-3.
    05 TERM-COUNT PIC 9(3).
    05 STATUS-CODE PIC X(2).
