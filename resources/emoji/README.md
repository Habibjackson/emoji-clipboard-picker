# Emoji catalog data

`emoji-test.txt` is the Unicode Emoji 18.0 keyboard/display test data, dated 2026-04-30 in its header. It was downloaded from [Unicode's public emoji directory](https://www.unicode.org/Public/emoji/latest/emoji-test.txt) on 2026-10-17. The SHA-256 checksum of the copy in this repository is:

```text
8f3735cda1f92a779d78af67cf86066bb1f07143dc22f2ac29394d9bc57ab21a
```

Unicode data is licensed under the Unicode License v3. Keep [UNICODE-LICENSE.txt](UNICODE-LICENSE.txt) with redistributed copies of this data. The application uses fully qualified sequences, group names, and fallback English names from this file; it does not bundle emoji artwork or fonts.

`cldr-en-48.2.tsv` contains English short names and keywords from the stable CLDR 48.2 release. It was generated from [English annotations](https://github.com/unicode-org/cldr/blob/release-48-2/common/annotations/en.xml) and [derived English annotations](https://github.com/unicode-org/cldr/blob/release-48-2/common/annotationsDerived/en.xml), matching their code points to the fully qualified sequences in `emoji-test.txt`. The SHA-256 checksums are:

```text
8511aadd046fdba2f0ffe590266ced8bbf48175ad139b2675d85d7141057b235  annotations/en.xml
d76bd041c8c9e7b00b716aff8b7d9dbf509877010e191d5efd068be2553e066e  annotationsDerived/en.xml
64e850ed7696b87706e888e7bfc26eb18b3412246a7d7b81cfad48e53268f3e7  cldr-en-48.2.tsv
```

Regenerate the bundled TSV from those pinned XML files:

```sh
curl -fsSL https://raw.githubusercontent.com/unicode-org/cldr/release-48-2/common/annotations/en.xml -o /tmp/cldr-en.xml
curl -fsSL https://raw.githubusercontent.com/unicode-org/cldr/release-48-2/common/annotationsDerived/en.xml -o /tmp/cldr-en-derived.xml
python3 scripts/generate_cldr_annotations.py resources/emoji/emoji-test.txt /tmp/cldr-en.xml /tmp/cldr-en-derived.xml resources/emoji/cldr-en-48.2.tsv
```

CLDR 48.2 supplies names and keywords for 3,944 of the 3,963 catalog entries. The 19 Emoji 18.0 additions without annotations use their names from `emoji-test.txt`. CLDR data is also under the Unicode License v3 in [UNICODE-LICENSE.txt](UNICODE-LICENSE.txt).
