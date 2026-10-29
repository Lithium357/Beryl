# IBM/z/OS status

IBM Enterprise COBOL and z/OS support is **not implemented or validated**. The executable runner currently targets Linux. Passing GnuCOBOL tests, using IBM-flavored compiler options, or running an older mainframe emulator would not establish Enterprise COBOL/z/OS compatibility.

The access investigation now identifies the [IBM ZLPN Validation Program](https://zlpn.ibm.com/docs/validation/program-description/), which offers short-term no-charge access for application porting/testing. IBM's [FAQ](https://zlpn.ibm.com/docs/faq/) directs applications and enrollment questions to `zlpn@ibm.com`. An independent FOSS vendor should ask IBM about eligibility, Partner Plus enrollment, and access to Enterprise COBOL and a C compiler through UNIX System Services. No z/OS project-validation environment has been obtained.

IBM Z Xplore is a learning environment; training access alone does not establish permission to run this project's workloads. An IBMid and a downloaded Linux x86 compiler trial also do not provide z/OS access. The Linux evaluation currently reports an expired evaluation license; see VENDOR_ACCESS.md for the exact tested archive and diagnostic.

An adapter needs a real authorized environment and compiler before compatibility can be claimed. Validation must cover callable-driver linkage, compiler/version/options, USS versus data-set allocation, fixed/variable record storage, character encoding, signed DISPLAY and packed-decimal conventions, statuses/abends, reproducibility and platform-specific process/resource control. Current ASCII codecs and Linux process-group/rlimit behavior are not mainframe emulation. This access/validation requirement remains the external blocker.
