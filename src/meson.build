src = [
        'git-annotated-source.c',
        'git-commit.c',
        'git-commit-bag.c',
        'git-commit-dialog.c',
        'git-commit-link-button.c',
        'git-common.c',
        'git-main-window.c',
        'git-reader.c',
        'git-source-view.c',
        'main.c',
]

enum_headers = [
        'git-annotated-source.h',
        'git-commit.h',
        'git-commit-bag.h',
        'git-commit-dialog.h',
        'git-commit-link-button.h',
        'git-common.h',
        'git-main-window.h',
        'git-reader.h',
        'git-source-view.h',
]

marshal = gnome.genmarshal('git-marshal',
        sources: 'git-marshal.list',
        prefix: '_git_marshal')

enums = gnome.mkenums('git-enum-types',
        h_template: 'git-enum-types.h.in',
        c_template: 'git-enum-types.c.in',
        sources: enum_headers)

built_src = [
        marshal[0], enums[0],
]

blame_browse = executable('blame-browse', src + built_src,
        dependencies: [gtk_dep],
        include_directories: configinc,
        install: true)
