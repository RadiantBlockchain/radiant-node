# Translations

The Bitcoin Cash Node graphical user interface is translated on Crowdin:

  <https://crowdin.com/project/bitcoin-cash-node>

## Contributing to translations

### Step 1: Join the Crowdin project

To contribute to translations, you must create a Crowdin account and join the
[BCHN project on Crowdin](https://crowdin.com/project/bitcoin-cash-node).

### Step 2: Activate your language

If you already see your language in the
[BCHN project on Crowdin](https://crowdin.com/project/bitcoin-cash-node), you
can proceed with the next step.

If not, then a maintainer first needs to turn on your language in the Crowdin
project. To do so, we need at least one proofreader for the language (see
*Proofread translations* below).

Note that the BCHN software contains translations in languages that are not
currently active in Crowdin. Software users can still use those translations,
but at the moment they are not maintained.

### Step 3: Translate strings

On [Crowdin](https://crowdin.com/project/bitcoin-cash-node), click on your
language and then on `bitcoin_en.ts` to begin translating. Select a source
string you want to translate and enter your translation.

At the bottom of the screen, you will find helpful automatic suggestions. Click
on a suggestion to use it, and adapt it as needed. The suggestions consist of
translation memory and machine translations. Translation memory is previous
translations of similar source strings, which includes other translations within
BCHN,
[imported translations from Bitcoin Core](https://crowdin.com/project/bchn-copy-of-bitcoin-core),
and other translations from other software projects on Crowdin. Machine
translations are computer-generated suggestions.

For inspiration, you can also see how a string was translated into other languages.

Ensure that terminology is consistent throughout your language. You can use the
search functionality to find out how particular technical terms were translated
in other sentences in the same language.

### Step 4: Proofread translations

For quality assurance purposes, every proposed translation must be verified by a
proofreader before it can be included in the BCHN software. To do so, you first
need to obtain the proofreader permission for your language. Contact us if you
are a member of the Bitcoin Cash community we can trust and you want to be a
proofreader for a particular language.

As a proofreader, you can click the checkmark to mark a translation as verified.
Verifying a translation means that it is good enough for inclusion in our
software: if unverified, the original American English text will be shown to
users instead. It is fine to approve your own translation proposals. If needed,
proofreaders can always change previously verified translations at a later
moment.

## Managing translations

### Exporting source strings from Git to Crowdin

We use automated scripts to help extract translations in both Qt, and non-Qt
source files. It is rarely necessary to manually edit the files in
`src/qt/locale/`. The translation files adhere to the format `bitcoin_xx_YY.ts`
or `bitcoin_xx.ts`.

`src/qt/locale/bitcoin_en.ts` is used as the source for all other translations.
To regenerate the `bitcoin_en.ts` file, run the following commands:

```sh
cd <build-dir>
ninja translate
```

Create a Merge Request to update the regenerated files in our Git repository.
You can use the following commands to create the commit:

```sh
git add src/qt/bitcoinstrings.cpp src/qt/locale/bitcoin_en.ts
git commit
```

On Crowdin, go to
[*Settings*, *Files*](https://crowdin.com/project/bitcoin-cash-node/settings#files),
click *Update*, and upload the new version of `bitcoin_en.ts` you just
generated. Translators can now begin translating new/modified source strings.

#### Notes

* A custom script is used to extract strings from the non-Qt parts. This script
  makes use of `gettext`, so make sure that utility is installed (i.e.,
  `apt-get install gettext` on Ubuntu/Debian). Once this has been updated,
  `lupdate` (included in the Qt SDK) is used to update `bitcoin_en.ts`.
* `contrib/bitcoin-qt.pro` takes care of generating `.qm` (binary compiled)
  files from `.ts` (source files) files. It’s mostly automated, and you
  shouldn’t need to worry about it.
* For general MRs, you shouldn’t include any updates to the translation source
  files. They will be updated periodically in separate MRs. This is important to
  avoid translation-related merge conflicts.

#### Plurals

When new plurals are added to the source file, it’s important to do the
following steps:

1. Open `bitcoin_en.ts` in Qt Linguist (included in the Qt SDK).
2. Search for `%n`, which will take you to the parts in the translation that
   use plurals.
3. Look for empty `English Translation (Singular)` and
   `English Translation (Plural)` fields.
4. Add the appropriate strings for the singular and plural form of the base
   string.
5. Mark the item as done (via the green arrow symbol in the toolbar).
6. Repeat from step 2, until all singular and plural forms are in the source
   file.
7. Save the source file.

### Importing verified translations from Crowdin into Git

On Crowdin, go to
[*Settings*, *Translations*](https://crowdin.com/project/bitcoin-cash-node/settings#translations),
and click *Build & Download*. Replace the files in the `src/qt/locale/`
directory with the files you just downloaded from Crowdin (some may need to be
renamed). Commit the changes and submit them as a Merge Request.

Since *Export only approved translations* is enabled in the Crowdin project
settings, any unverified translations won’t be included in your download.

### Updating Bitcoin Core translation memory in Crowdin

In order to provide translators with helpful automatic suggestions based on
previous translation work in Bitcoin Core, BCHN maintains a copy of Bitcoin
Core’s translations in a separate Crowdin project:

  <https://crowdin.com/project/bchn-copy-of-bitcoin-core>

To update these:

1. Go to
   [*Settings*, *Files*](https://crowdin.com/project/bchn-copy-of-bitcoin-core/settings#files),
   and upload `src/qt/locale/bitcoin_en.ts` from the
   [Bitcoin Core Git repository](https://github.com/bitcoin/bitcoin).
2. Go to
   [*Settings*, *Translations*](https://crowdin.com/project/bchn-copy-of-bitcoin-core/settings#translations),
   and upload the other `src/qt/locale/bitcoin_*.ts` files.
3. Go to
   [*Settings*, *General*](https://crowdin.com/project/bchn-copy-of-bitcoin-core/settings#general)
   and change the version number to the last Bitcoin Core release.

### Registering languages in Bitcoin-Qt

To create a new language template, you will need to edit the languages manifest
file `src/qt/bitcoin_locale.qrc` and add a new entry. Below is an example of the
English language entry.

```xml
<qresource prefix="/translations">
    <file alias="en">locale/bitcoin_en.qm</file>
    ...
</qresource>
```

Note that the language translation file must end in `.qm` (the compiled
extension), and not `.ts`.
