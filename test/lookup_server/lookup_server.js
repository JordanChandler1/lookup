// lookup-client
// Author: Jordan Chandler

// Simple node based web api server rate limited to 5 ongoing requests.
//  API syntax  localhost[:port]/item/:id

// A request takes 2 seconds to complete.
// Status 429 and a json status returned if rate limit is exceeded.
// Otherwise json item with requested information is returned.

// Test with curl
// curl -s -o /dev/null -w "%{http_code}" http://localhost:3000/items/123 -H "Authorization: Y1JGMmR2RFpRc211MzdXR2dLNk1UY0w3WGpl"

const http = require('http');
const { URL } = require('url');

let route = "/items/";
let port = 8080;
let authorization_token; 
let timeout = 0;

// Parse arguments
const argv = require('yargs/yargs')(process.argv.slice(2))
    .usage('Usage: $0 [-p num] [-t num]')
    .help('help').alias('help', 'h')
    .option('p', {
        alias: 'port',
        demandOption: false,
        default: 8080,
        describe: 'TCP/IP port number that server will monitor for requests.',
        type: 'number',
        nargs: 1
    })
    .option('r', {
        alias: 'route',
        demandOption: false,
        default: '/items/',
        describe: 'Relative URL route to monitor.',
        type: 'string',
        nargs: 1
    })
    .option('a', {
        alias: 'authorization',
        demandOption: true,
        describe: 'Authorization token expected in HTTP authorization header.',
        type: 'string',
        nargs: 1
    })
    .option('t', {
        alias: 'time',
        demandOption: false,
        default: 0,
        describe: 'Time in milliseconds to process each request',
        type: 'number',
        nargs: 1
    })
    .strict()
    .argv

port = argv.p;
route = argv.r.trim();
authorization_token = argv.a.trim();
timeout = argv.t;

if (!route.startsWith("/"))
{
    route = "/" + route;
}
if (!route.endsWith("/"))
{
    route = route + "/";
}

const routeParts = route.split("/");
if (routeParts.length > 3)
{
    console.log ("lookup-server routes must be top level routes with only one component");
    return;
}
routeParts.filter(function(value, index, arr){ 
    return value.trim() !=="";});
 
route = routeParts.join("");   

console.log("lookup-server listening for .../" + route + "/:id on port " + port + " requiring authorization token " + authorization_token + " with processing time " + timeout + ".\n")

global.requestCount = 0;

const app = http.createServer((request, response) => {
    const query = new URL(request.url, "http://localhost/");
    const pathName = query.pathname;
    const pathParts = pathName.split("/");

    // Ignore non-route queries - return status NOT FOUND
    if ((pathParts.length > 0) && (pathParts[1] !== route)) {
        response.writeHead(404, { "Content-Type": "text/json" });
        response.end();
        return;
    }

    // Limit simultaneous requests - return status TOO MANY REQUESTS
    if (++global.requestCount > 5) {
        response.writeHead(429, { "Content-Type": "text/json" });
        response.end();
        global.requestCount--;
        return;
    }

    // Verify base64 encoded authorization header
    if ((request.headers.authorization || "") != authorization_token) {
        // Authentication header not
        response.writeHead(403, { "Content-Type": "text/json" });
        response.end();
        global.requestCount--;
        return;
    }

    // Process requests
    setTimeout(() => {
        if ((pathParts.length > 2) && (pathParts[2] != "")) {
            // Requests fulfiulled - return status OK and JSON payload
            let item = { result: "Item is in inventory." };
            response.writeHead(200, { "Content-Type": "text/json" });
            response.write(JSON.stringify(item));
        } else {
            // Item not provided - return status NOT FOUND
            response.writeHead(404, { "Content-Type": "text/json" });
        }
        response.end();
        global.requestCount--;
    }, timeout)
});

app.listen(port);