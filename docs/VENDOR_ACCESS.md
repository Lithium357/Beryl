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
`cob2 -V` identifies version **1.2.0.9**, built 2026-01-05. An initial local
extraction reported an expired evaluation license. This was a setup error: the
evaluation package's normal `postinst` runs IBM's `xlcmp` initialization utility,
which extraction alone omitted. Installing the supplied Ubuntu runtime,
evaluation-license and compiler packages normally with `dpkg -i` at their
default `/opt/ibm` paths resolved the diagnostic. The supplied evaluation media
then compiled the accounts fixture successfully. No replacement media was
needed, and no license data, system clock or license checks were modified.

The compiler's diagnostic and supplied README list
`COBOL.Linux.Trial@ca.ibm.com` for actual trial questions. The local Ubuntu/WSL
host is outside the specific Ubuntu releases advertised on IBM's product page;
local compatibility results do not establish vendor-certified host support.

Linux x86 compiler validation would still be separate from z/OS validation.

## Rocket Visual COBOL

The [Personal Edition page](https://www.rocketsoftware.com/en-us/products/cobol/visual-cobol-personal-edition)
offers free non-commercial use, describes the product as academic, and advertises
a one-year student developer license. Its
[download request page](https://www.rocketsoftware.com/en-us/products/cobol/visual-cobol-personal-edition/free-download)
provides the route to request the software.

Beryl compatibility development is not automatically covered by a personal
educational license simply because Beryl is free software. On 2026-10-29, with
the user's explicit approval, the Ubuntu Personal Edition request was submitted
through Rocket's official form. The request identifies Beryl as an independent
FOSS testing tool and asks whether the license permits compatibility development
and publication of results, or whether another evaluation license is required.
Rocket's page confirmed successful submission and stated that its team would
reply. Contact details are not stored in this repository. Installer delivery
and the licensing clarification remain pending; no Rocket compiler has been
installed and no Rocket compatibility result is claimed.

## IBM z/OS

The [IBM ZLPN Validation Program](https://zlpn.ibm.com/docs/validation/program-description/)
offers short-term no-charge systems for porting/testing. IBM's
[enrollment FAQ](https://zlpn.ibm.com/docs/faq/) lists `zlpn@ibm.com` for applications.
Request z/OS, UNIX System Services, Enterprise COBOL, and a suitable C compiler;
availability and eligibility require IBM confirmation. See IBM_ZOS_STATUS.md.
