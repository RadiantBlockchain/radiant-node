# Responsible Disclosure Policy

Bitcoin Cash Node takes security very seriously.  We greatly appreciate any and
all disclosures of bugs and vulnerabilities that are done in a responsible
manner.  We will engage responsible disclosures according to this policy and put
forth our best effort to fix disclosed vulnerabilities as well as reaching out
to numerous node operators to deploy fixes in a timely manner.

This disclosure policy is also intended to conform to [this proposed standard](https://github.com/RD-Crypto-Spec/Responsible-Disclosure/blob/184391fcbc1bbf3c158c527a841e611ac9ae8388/README.md)
with some modifications (see below).

## Responsible Disclosure Guidelines

Do not disclose any security issue or vulnerability on public forums, message
boards, mailing lists, etc. prior to responsibly disclosing to Bitcoin Cash Node
and giving sufficient time for the issue to be fixed and deployed.
Do not execute on or exploit any vulnerability.  This includes testnet, as both
mainnet and testnet exploits are effectively public disclosure.  Regtest mode
may be used to test bugs locally.

## Reporting a Bug or Vulnerability

When reporting a bug or vulnerability, please provide the following to
security@bitcoincashnode.org:

* A short summary of the potential impact of the issue (if known).
* Details explaining how to reproduce the issue or how an exploit may be formed.
* Your name (optional).  If provided, we will provide credit for disclosure.
  Otherwise, you will be treated anonymously and your privacy will be respected.
* Your email or other means of contacting you.
* A PGP key/fingerprint for us to provide encrypted responses to your disclosure.
  If this is not provided, we cannot guarantee that you will receive a response
  prior to a fix being made and deployed.

## Encrypting the Disclosure

We highly encourage all disclosures to be encrypted to prevent interception and
exploitation by third-parties prior to a fix being developed and deployed.
Please encrypt using the PGP public key with fingerprint:
`15BB B66E ACA6 F8B2 FE56  9445 7A90 DE1B F996 8031`

It may be obtained via:

```
gpg --recv-keys "15BB B66E ACA6 F8B2 FE56  9445 7A90 DE1B F996 8031"
```

After this succeeded you can encrypt your prepared disclosure document with the
following line:

```
gpg --armor --encrypt --recipient security@bitcoincashnode.org yourDisclosureDocument
```

This will create a copy of your document with the file extension `.asc`, you can
email this encrypted version of the document to Bitcoin Cash Nodes security team.

If you still have questions about how to do it, feel free to contact the Bitcoin
Cash Node project security team for more instructions.

## Backup PGP Keys

These PGP fingerprints and emails are provided only as backups in case you are
unable to contact Bitcoin Cash Node via the security email above.

### freetrader

```
Bitcoin Cash Node Lead Maintainer
contact at freetrader at tuta dot io
CC329A4FB0E41392829505FEC07A7C345E86B06C
```

### emergent_reasons

```
Bitcoin Cash Node Maintainer
contact at emergentreasons at gmail dot com
3F4F4CFA80488BA38DD45FF042D1A8643349D91F
```

### Dagur Valberg Johannsson

```
Bitcoin Cash Node Developer and Maintainer
contact at dagurval at pvv dot ntnu dot no
7AA5D836BCC067566039C49425A0F479C0F086B6
```

### imaginary_username

```
Bitcoin Cash Node Developer and Maintainer
contact at im_uname at bitcoincashnode dot org
B645B35C2B1D9AAC4D166E251BE4DFC04D72C281
```

### Calin Culianu

```
Bitcoin Cash Node Developer and Maintainer
contact at calin dot culianu at gmail dot com
D465135F97D0047E18E99DC321810A542031C02C
```

## Disclosure Relationships

Neighboring projects that may be affected by bugs, potential exploits, or other
security vulnerabilities that are disclosed to Bitcoin Cash Node will be passed
along information regarding disclosures that we believe could impact them. As
per the standard referenced above, we are disclosing these relationships here:

* [Bitcoin Unlimited](https://www.bitcoinunlimited.info)
    * Security Contacts: security at bitcoinunlimited dot info
    * Disclosure Policy: [https://github.com/BitcoinUnlimited/BitcoinUnlimited/blob/release/SECURITY.md](https://github.com/BitcoinUnlimited/BitcoinUnlimited/blob/release/SECURITY.md)

* [BCHD](https://bchd.cash)
    * Security Contacts: Chris Pacia (ctpacia at gmail dot com) and Josh Ellithorpe
      (quest at mac dot com)
    * Disclosure Policy: see README information at [https://github.com/gcash/bchd/](https://github.com/gcash/bchd/)

* [Flowee](https://flowee.org)
    * Security Contact: tomz at freedommail dot ch
    * Disclosure Policy: see [https://gitlab.com/FloweeTheHub/thehub](https://gitlab.com/FloweeTheHub/thehub)

* [Knuth](https://github.com/k-nuth/kth/)
    * Security Contact: fpelliccioni at gmail dot com
    * Disclosure Policy: see [https://github.com/k-nuth/kth/blob/master/README.md#security-disclosures](https://github.com/k-nuth/kth/blob/master/README.md#security-disclosures)

* [Bitcoin Verde](https://github.com/SoftwareVerde/bitcoin-verde/)
    * Security Contact: josh at softwareverde dot com

We have approached several other projects and are waiting for responses from them.

If you feel your projects is closely related and would like to form a disclosure
relationship with Bitcoin Cash Node, please contact our security email address
to discuss.

## Bounty Payments

Bitcoin Cash Node cannot at this time commit to bounty payments ahead of time.
However, we will use our best judgement and do intend on rewarding those who
provide valuable disclosures (with a strong emphasis on easy to read and
reproduce disclosures).

## Deviations from the Standard

While Bitcoin Cash Node believes that strong cohesion among neighoring projects
and ethical behavior can be standardized to reduce poorly handled disclosure
incidents, we also believe that it's in the best interest of Bitcoin Cash for us
to deviate from the standard in the following ways:

* The standard calls for coordinated releases. While Bitcoin Cash Node will make
  attempts to coordinate releases when possible, it's not always feasible to
  coordinate urgent fixes for catastrophic exploits (ie. chain splitting events).
  For critical fixes, Bitcoin Cash Node will release them in the next release
  when possible.

## Making changes to this disclosure policy

Note that any changes to this disclosure policy should be mirrored in a pull
request to the [Bitcoin Cash Node repository](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node).
