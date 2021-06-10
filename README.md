# udtc: UTF8 Double Transposition Cipher

This program encodes UTF8 text files with a double transposition cipher.

See the included man page for option details.

## Installation

Clone this repository, then:

    $ make PREFIX=/usr/local install

This will install both the compiled binary and a manual page under `PREFIX`.

## Example

    $ echo -n PROGRAMMINGPRAXIS | udtc
    key1: COACH
    key2: STRIPE
    GNPAPARSRIMOIXMGR

    C O A C H  -->  S T R I P E  -->  GNPAPARSRIMOIXMGR
    P R O G R       O M R P A G
    A M M I N       I G I A R N
    G P R A X       X R M P S
    I S

    $ echo -n GNPAPARSRIMOIXMGR | udtc -d
    key1: COACH
    key2: STRIPE
    PROGRAMMINGPRAXIS

## Example with UTF8 characters:

    $ echo -n bête-et-discipliné | udtc
    key1: COACH
    key2: STRIPE
    e-nédtbpcieêtsi-il

    C O A C H  -->  S T R I P E  -->  e-nédtbpcieêtsi-il
    b ê t e -       t - i é b e
    e t - d i       s i e d p -
    s c i p l       i l ê t c n
    i n é

    $ echo -n e-nédtbpcieêtsi-il | udtc -d
    key1: COACH
    key2: STRIPE
    bête-et-discipliné

