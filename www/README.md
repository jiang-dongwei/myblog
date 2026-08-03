# GP2040-CE Web Configurator

Simple web application for gamepad configuration.

For a source-backed overview of the implemented pages, firmware data flow,
storage behavior, and modification points, see
[`../docs/WEB_CONFIG_FUNCTIONAL_AND_DEVELOPMENT_GUIDE.md`](../docs/WEB_CONFIG_FUNCTIONAL_AND_DEVELOPMENT_GUIDE.md).

## Requirements

* NodeJS and NPM to build the React app

## Development

### Mocked board

Run `npm run dev`. This will start up the React app and an Express instance for mock data during development, allowing testing of the configurator without loading it onto the MCU, which is a SLOW process.

The mock data Express server is running at <http://localhost:8080>.

### Connected board

Run `npm run dev-board`. This starts the React app locally and sends its API
requests to the connected board at `http://192.168.7.1`. The board must already
be running in Web Config mode.

### API Endpoints

When adding a new API endpoint to the GP2040-CE Configurator:

> NOTE: All endpoints should be under the `/api` path

* Add the handler to `../src/webconfig.cpp` and register the `/api/...` path in
  `handlerFuncs` or `handlerFuncsWithStatusCode`.
* Add a mock data endpoint to `server/app.js`.
* Add the client-side API function to `src/Services/WebApi.js`.
* Add the endpoint to the Postman collection at `server/docs/GP2040-CE.postman_collection.json`.

### Files

Use JPG and PNG files for images, SVG file support requires modification to the lwIP library that hasn't been completed.

## Building

If you just want to rebuild the React app in production mode for some reason, you can run `npm run build` from the `www` folder.

The `makefsdata.js` script is used to build the React application and regenerate the embedded data in `lib/httpd/fsdata.c`. The `makefsdata` tool that performs the conversion doesn't set the correct `#include` lines for our use. This script will fix this issue.

Precompiled binaries of `makefsdata` for Windows, Linux and macOS are included in the `tools` folder.

## Bundle size

After adding any type of dependency please check the production bundle size either by running `npm run build` or a more detailed breakdown `npm run analyze`

## References

Original example:

* <https://forums.raspberrypi.com/viewtopic.php?t=306888>
* <https://github.com/maxnet/pico-webserver>

Convert text to bytes:

* <https://onlineasciitools.com/convert-ascii-to-bytes>
* <https://onlineasciitools.com/convert-bytes-to-ascii>

Create image map:

* <https://www.image-map.net/>
