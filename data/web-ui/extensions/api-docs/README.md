# API reference – every route of /api/v1, to read and try

`api-docs` shows the box's own description of its interface,
`/api/openapi.json`, in the Swagger UI viewer: every route with what it
takes, what it answers and the level it needs, and a form to try it on
the box.

## What it needs

The viewer itself is not part of the extension. It comes with the
optional package `swagger-ui`, which puts its files under
`/swagger/vendor/` of the web folder; an image carries it only where its
flash has room. Without the package the page says so and shows nothing
else. The description it reads is the box's own and always matches the
build.

The extension is off until it is switched on under Settings →
Extensions; it then shows as a tile under Tools.

## Release

```sh
mkdir -p /tmp/stage/api-docs && \
rsync -a --exclude .git --exclude .github ./ /tmp/stage/api-docs/ && \
tar -czf api-docs.tar.gz -C /tmp/stage api-docs
```
