# QSearchable

[![Unit Tests](https://github.com/MrEricSir/QSearchable/workflows/CI/badge.svg)](https://github.com/MrEricSir/QSearchable/actions)

Integrates desktop search (Spotlight, Windows Search, etc.) into your Qt application.

For full details see [the documentation.](https://mrericsir.github.io/QSearchable/qsearchable-module.html)

This library was originally developed for the [Fang newsreader](https://github.com/MrEricSir/Fang).

## Quick Start

```cpp
#include "QSearchableIndex.h"
#include "QSearchableItem.h"

// .. etc.

// Create items from your application's content.
QSearchableItem item("item-123");
item.setTitle("Example Item");
item.setContentDescription("This is the text associated with an item.");
item.setDomainIdentifier("items");

// Index the items.
auto *index = QSearchableIndex::Get();
index->indexItems({item});

// Optionally, listen for a signal that the indexing is complete.
connect(index, &QSearchableIndex::indexingSucceeded, [](int count) {
    qDebug() << "Indexed" << count << "items";
});

// Handle activation from the platform search.
connect(index, &QSearchableIndex::activated, [](const QString &id) {
    qDebug() << "User activated item:" << id;
});
```

## How To Include

The most straightforward way to include QSearchable is with git submodule.

In the root directory of your existing project's git repository:

```bash
git submodule add git@github.com:MrEricSir/QSearchable.git external/QSearchable
git add .gitmodules
git commit -m "Add QSearchable submodule"
```

This will place QSearchable into external/QSearchable.

Add the submodule into your application's CMakeLists.txt:

```cmake
add_subdirectory(external/QSearchable)
target_link_libraries(YourApp PRIVATE QSearchable::QSearchable)
```

## Building and Running

Build QSearchable, the example app, and unit tests:

```bash
cmake -B build
cmake --build build
```

Run the unit tests:
```bash
ctest --test-dir build
```

The example app will be in the build folder; run it on MacOS with:
```bash
open ./build/example/QSearchableListDemo.app
```

## Platform Note for MacOS

**Important:** Read the following carefully as no errors will be surfaced if you skip these steps.

The app **must** be in a signed bundle for indexed items to appear in Spotlight.

For local development, ad-hoc signing is sufficient (no Apple Developer account required):

```bash
codesign -s - --force --deep YourApp.app
```

For distribution use your Developer ID certificate.
