
# Lookup

The **Lookup** project is a demonstration of a multi-threaded web client that demonstrates gate-limited web requests to a hypothethical authenticated RESTful web service.  For unknown reasons, the web service can only look up a single item per request. Further compounding its scalability, the service is also limited to 5 simultaneous requests. The service returns an HTTP 429 status for any request received while its has 5 or more requests underway.

In spite of the service's limitations, the company's software developers need to make many web requests per day.  These requests may include duplicate requests. The company wants a high performance utility that hides the complexity of successfully interacting with the web service.  The utility must:

- Be callable from another program
- Retrieve item information as quickly as possible
- Avoid server overrun (i.e., HTTP 429) responses
- Not issue requests for items previously retrieved
- Supply the required authentication/authorization credentials

The Lookup project is composed of:

- **lookup-get** - A C++ class that performs multi-thread gate limited RESTful web api requests using a caller supplied base URL.
- **lookup-server** - A Node.js RESTful web api service that simulates the company's gate limited web service.
- **lookup-client** - A C++ console application that simulates the company's existing programs that need to make effecient web api calls to the company's web service.

## Getting Started

### Install prerequsites

To compile and run this project you need to install:
- [Node.js](https://nodejs.org/)
- [G++](https://gcc.gnu.org/)
- [libcurl](https://curl.se/download.html)

then:

1. Clone the respository at [lookup's GitHub repo](www.github.com)
2. Follow the steps below.

### Start lookup-server

In lookup/test/lookup/server, run the server by executing:

```
        node lookup_server.js -p 8080 -r /items/ -a Y1JGMmR2RFpRc211MzdXR2dLNk1UY0w3WGpl
```

to listen on port 8080 for requests to .../items/:id with authorization header Y1JGMmR2RFpRc211MzdXR2dLNk1UY0w3WGpl

To see the lookup_server's available parameters and defaults, execute:
```
        node look_server.js -h
```
which produces the follwing output:
```
        Usage: lookup_server.js [-p num] [-t num]

Options:
      --version        Show version number                             [boolean]
  -h, --help           Show help                                       [boolean]
  -p, --port           TCP/IP port number that server will monitor for requests.
                                                        [number] [default: 8080]
  -r, --route          Relative URL route to monitor.
                                                   [string] [default: "/items/"]
  -a, --authorization  Authorization token expected in HTTP authorization
                       header.                               [string] [required]
  -t, --time           Time in milliseconds to process each request
                                                           [number] [default: 0]
```

### Compile lookup-client

1. In lookup/test/client, compile lookup-client.cpp to a console applicaion by executing:
 
```
        /usr/bin/g++ 
            -std=c++17 
            -I../../src 
            lookup-client.cpp 
            -lpthread 
            -L/usr/lib/curl 
            -lcurl 
            -o ./lookup-client

```
### Start lookup-client 

In /lookup/test/lookup_client, run the client using parameters that corrspond to the parameters used to run the server.  

Note that the switch names may be differentare from lookup_server. 

```
        lookup-client -u <url> -a <authorization token>
```

which instructs lookup_client to make HTTP GET requests to the specified url with the specified authorization token in the HTTP authorization header.  All other parameters will use their default values.  To see the available parameters and defaults, execute:
```
lookoup_client -h
```
which results in the following:
```
lookup_client -Url <url> [-Port port] [-Authorization token] [-Requests count] [-Limit limit]

Items not enclosed enclosed in <> are required.  Items enclosed in [] are optional.If optional switches are not provided the following defaults are used:
    [port]:   8080
    [token]:
    [count]:  100
    [limit]:  5

Notes:
  Switches may be abbreviated using the first letter of the switch.
  Switches may be any combination of upper case and lower case letters.
  Switches may omitted and the value will be determined positionally.
  Switches may be reordered and any value without a switch will be used to fulfill a remaining positional value.
```

### Understand how it works

lookup_client prepares the number of requests specified in the -r parameter.  For each request, it adds an adjacent duplicate request to test lookup_get's ability to avoid reissuing close spaced duplicate requests.  For good measure, the entire request list is duplcated and appended.  This tests non-closedly space duplicate requests.

lookup_client delagates the work of issuing the requests to the lookup_get class.

lookup_get's request() method loads the requests into a shared mutex protected queue and spins up one threaded requestor() for each simultaneous request allowed by the web service (and specifed in the -Limit argument).

Each requestor removes an item from the request queue and checks a in-process cache of previously received responses.  If the item id is found in the cache, no request is made.  If the item id is not the cache, a reservation is added to the cache.  From this moment on, other threads will see this item is as having already been received and will not issue duplicate requests.

The request then waits on a shared semaphore for an open request slot.  On started, requests slots are initialize at the web service's gate limit.  When a slot becomes available, the requestor() receives this slot and the slot beconmes unavailable to other requestor()s.  When requestor()s complete the request, the request slot is released.  In this way, lookup_get prevents web service overruns.  The client should not receive a status code 429 if lookup_client is the only user of the web service.  If 429's are received, lookup_get handles it by rolling back the request so it can be retried by other threads.

The requestor then issues the request.  lookup-server returns a a JSON response payload that indicates if the item was found (in inventory as an example).  

The request handling is dictated by the HTTP status code returned by the web service.

- **Status 200 (OK)** : The previously reserved cache entry is updated with the timestamp, HTTP status code, and the response payload from the web service.
- **Status 403 (NOT AUTHORIZED)** :  The previously reserved cache entry is updated with the timestamp, HTTP status code, and a NULL response payload.
- **Status 404 (NOT FOUND)** : The previously reserved cache entry is updated with the timestamp, HTTP status code, and a NULL response payload.
- **Status 429 (RATE LIMIT EXCEEDED)** : The cache reservation for the item is removed.  A compensating transaction to the request queue is made to rollback the removal of the item from the request queue.
- **All Other Status Codes** : The previously reserved cache entry is updated with the timestamp, HTTP status code, and a NULL response payload.

When the processing of this request is completed, the requestor() releases the request slot so other threads can run.

When the request queue is empty, the requstor() returns to its caller causing the associated thread to terminate.

After all requests are completed and all requestor() threads terminated, lookup_get's request() method, returns an array of the cached response data in a JSON format that encapulates lookup_server's JSON response data.

For each unique item requested, the returned data includes the item id, a high resolution UNIX timestamp epoch, the HTTP status code, and the encapsulated response payload from the web service.

The data returned for a single item id request is shown below.

```
{
    "id": "02P15UREpTrkKclgf34SYLzGbhyt9sVe",
    "timestamp": 1619130745675723300,
    "status": 200,
    "response": {
        "result": "Item is in inventory."
    }
}
```

### Lookup Internals

Lookup attempts to acheive high performance by:

1. Using a high performance language for lookup_get, the threaded web requestor.  In this case, C++.
1. Using a lightweight web request framework.  In this case, libcurl is used to issue web requests.
1. Using a fast semaphore implementation for reference counting the request slots.  Using third-party source code (see lookup_get source code for details), lookup_get uses a fast_semaphore implementation that avoids mutexes and condition variables where possible.  Instead, atomic variable updates are used when possible with a fall back to a traditional semaphore implementation when required.
1. Lookup is provided in source code form as a CPP that can be included in the calling C++ program.  A single CPP is used primarily to allow the code to be easily understood.  For production, use other delivery formats (header files, libraries, etc.) would be used.

### Issues for further consideration

While this is a credible attempt at addressing the company's needs, due to time and other constraints no design is perfect. 

The following issues should be considered if further functional or performance improvements are being considered:

1. libcurl may have performance issues under heavy load.  Consider higher speed alternatives.
1. lookup_get requestor() builds response data using string concatenation which may result in string moves or reallocations hurting performance.
1. lookup_get request() receives the request data synchronously.  Accepting all incoming data must be done before the first thread can be spun up and released for making requests.  Effeciency can be improved if web request I/O can be overlapped with this setup time.  Consider asynchonusouly streaming the intiial requests data to lookup_get request().
1. lookup_get's requestor() threads expect the request queue to non-empty on startup and terminate immmediately when the queue runs dry.  This prevents lookup_get request() from streaming the data into the queue while waiting requestors() begin to process the data.  Consider moving to a producer/consumer queuing model to better overlap CPU and I/O.
1. lookup_get uses fast semaphores (normally atomics) to manage the request slots but standard mutexes to manage the shared request queue and reponse cache.  Consider using a fast binary sempahore for this purpose, some direct use of atomics, a some lock-free collection implementation.
1. The map data structure used for the response cache keeps the data sorted which is not required.  Consider using a lighter weight cache implementation.
1. If the web service rate limits were higher or if duplicate requests across request() calls were desired, the cache implementation might need to move outside of lookup_get. Consider a well designed interface which would make this more easiy done.
1. lookup_get's requestor() takes overhead to access the shared request queue and reponse cache.  This overhead is repeated for each item queried.  Consider popping multiple items off the queue on each pass to minimize the number of times the shared objects are locked.
1. Non-C++ clients cannot use lookup_get directly.  Considure a Python callable library implementation.
1. lookup_get's requestor() requeues items that receive a rate limit exceeded reposnse (429) and then immediately continue to remove and process items from the request queue.  Considure implementing flow control logic to backoff the request rate until (globally) the number of 429 responses decreases or some overall effeiciency emtric is improved.

