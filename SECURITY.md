# FreeRDP Security Policies and Procedures

This document describes the security policy and procedures for the [FreeRDP Project](https://github.com/FreeRDP/FreeRDP).
The following topics are covered:

  * [Supported Versions](#supported-versions)
  * [Reporting a Vulnerability](#reporting-a-vulnerability)
  * [Disclosure Procedure](#disclosure-procedure)


## Supported versions

Security is very important for us therefore we try to provide security updates and support for
the latest stable version as well as for the development branch.
Since our development branch is, like the protocol itself, a moving target we won't request CVEs for issues that are *only* found on the development branch.

The following table shows the currently supported versions:

| Version | Branch       | Supported          | 
| ------- |--------------| ------------------ |
| < 2.0.0 | stable-1.x   | :x:                |
| 2.x.x   | stable-2.0   | :heavy_check_mark: |
| -       | master       | :white_check_mark: |


## Reporting a vulnerability

**IMPORTANT**: Please, do not file security vulnerabilities as public issues on GitHub

In advance: **Thank you** for reporting a security vulnerability and making FreeRDP more stable! We really appreciate your effort.
Please let us know who we should give the credit or attributions to.


If you have found a security vulnerability in FreeRDP you can either directly open an [Advisory on GitHub](https://github.com/FreeRDP/FreeRDP/security/advisories/new)[^1] or send us an email to mailto:security@freerdp.com

In case of an email you can use the [FreeRDP security team GPG key](#reporting-gpg-key) for encrypted communication.

Once we receive a report we will review it and respond as soon as possible.

###


## Disclosure procedure

When the FreeRDP team receives a report one of the team members will be assigned as primary contact.
The primary contact will do all further communications and coordinate the fix and release process. 

How your report will be handled:

* When a report is received we will acknowledge the reception and review the reported issue(s) as soon as possible.
* Once confirmed we will determine the affected versions. If not reported via GitHub a [security advisory draft on GitHub](https://github.com/FreeRDP/FreeRDP/security/advisories) will be created for any issue. If it applies we will request a CVE.
* On a private branch we will fix the issue and check the code for any potential similar problem. 
* After the fix is validated we will create and publish a new release for all supported versions and publish the advisories.

## Reporting GPG key 

FreeRDP's security reporting public gpg key https://pub.freerdp.com/FreeRDP-security-team.pub.asc

```
-----BEGIN PGP PUBLIC KEY BLOCK-----

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
=O0kP
-----END PGP PUBLIC KEY BLOCK-----
```
[^1]: https://docs.github.com/en/code-security/security-advisories/guidance-on-reporting-and-writing/privately-reporting-a-security-vulnerability
