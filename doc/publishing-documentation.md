# Publishing the documentation

The documentation located in the `/doc` folder (and all of the git tree) is
automatically published to [docs.bitcoincashnode.org](https://docs.bitcoincashnode.org)
when changes are merged into the master branch.

This is done as part of the continuous integration, by the job named `pages`
in `.gitlab-ci.yml`.

The tool used to generate the HTML pages is [mkdocs](https://www.mkdocs.org).

## Adding documents to the navigation menu

While all documents in the git tree are published, they need to be
manually added to the navigation menu.

To do this, add it to the appropriate subsection under the `nav:` section
of `/mkdocs.yml`.

## Testing changes locally

You'll need to install mkdocs, the theme we use and plugins. You'll need to have
Python3 installed.

```
pip3 install mkdocs
pip3 install mkdocs-material
pip3 install mkdocs-exclude
```

Because of a detail of how mkdocs works, you need to create a folder with symlinks
to the root of the git tree.

```
mkdir files-for-mkdocs
cd files-for-mkdocs ; ln -s ../* . ; rm -rf files-for-mkdocs
rm -f files-for-mkdocs/{depends,src}
tar cf - $(find src depends -name \*.md) src/interfaces/*.h src/qt/intro.cpp src/chainparams.cpp | ( cd files-for-mkdocs ; tar xf - )
```

Notice that `depends` and `src` have been excluded, to reduce the size of the pages.
Files in those two directories need to be included manually (see the last line
in the code above).

You can serve the pages locally by running `mkdocs serve` from the git root.
This will start a local webserver at `http://127.0.0.1:8000`. To listen on a
different host/port, the parameter `-a` can be used.
Example: `mkdocs serve -a 10.0.0.10:1234`. You can also build the static html
files by running `mkdocs build`.

## Relative vs. absolute links

Due to the way `mkdocs` works, all links in the documentation need to be relative
and not absolute.

**Example**: Don't link to `/src/qt/intro.cpp` but do link to `../src/qt/intro.cpp`,
if the document you are linking *from* is in the `doc/` folder.
