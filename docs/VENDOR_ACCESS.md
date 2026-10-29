# Proprietary compiler access

Status checked on 2026-10-29. No proprietary compiler binaries, license files,
account details or evaluation archives belong in this repository. Local media
and build tools are kept under the ignored `.tools/` directory.

## IBM COBOL for Linux on x86

IBM advertises a [60-day non-production evaluation](https://www.ibm.com/products/cobol-compiler-linux-x86).
The [trial registration page](https://www.ibm.com/account/reg/us-en/signup?formid=urx-50118)
requires an IBMid. The user registered and supplied the official archive
`IBM_COBOL_V1.2.0_LINUX_EVAL.x86-64.260105.tar.gz`.

Archive SHA-256:
`d7922a5d716918252879d3050a346a17f5f0a3bc7a4f26415ba374a59fad861b`.
The Ubuntu packages were extracted locally for inspection. `cob2 -V` identifies
version **1.2.0.9**, built 2026-01-05. The first attempt to compile a fixed-format
copy of the portable accounts fixture exited unsuccessfully with:

```text
The license for the Evaluation version of IBM COBOL for Linux, 01.02.0000.0009 compiler product has expired.
```

The compiler's diagnostic and supplied README direct trial questions to
`COBOL.Linux.Trial@ca.ibm.com`. Current evaluation media or a resolution from IBM
is required before testing can continue. No IBM-compiled fixture or passed IBM
compatibility result exists. The local Ubuntu/WSL host is also outside the
specific Ubuntu releases advertised on IBM's product page, so an eventual
successful local test would not establish vendor-certified host support.

Linux x86 compiler validation would still be separate from z/OS validation.

## Rocket Visual COBOL

The [Personal Edition page](https://www.rocketsoftware.com/en-us/products/cobol/visual-cobol-personal-edition)
offers free non-commercial use, describes the product as academic, and advertises
a one-year student developer license. Its
[download request page](https://www.rocketsoftware.com/en-us/products/cobol/visual-cobol-personal-edition/free-download)
provides the route to request the software.

Beryl compatibility development is not automatically covered by a personal
educational license simply because Beryl is free software. Ask Rocket to confirm
the permitted evaluation route for an independent open-source tool vendor,
including Linux x86-64 compiler/runtime access and publication of compatibility
results. No request has been sent on the user's behalf, no Rocket compiler has
been installed, and no Rocket compatibility result is claimed.

## IBM z/OS

The [IBM ZLPN Validation Program](https://zlpn.ibm.com/docs/validation/program-description/)
offers short-term no-charge systems for porting/testing. IBM's
[enrollment FAQ](https://zlpn.ibm.com/docs/faq/) lists `zlpn@ibm.com` for applications.
Request z/OS, UNIX System Services, Enterprise COBOL, and a suitable C compiler;
availability and eligibility require IBM confirmation. See IBM_ZOS_STATUS.md.
