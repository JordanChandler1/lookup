#ifndef LOOKUP_GET_CPP_INCLUDED
#define LOOKUP_GET_CPP_INCLUDED

// #ifdef __cplusplus
// extern "C"
// {
// #endif

// lookup_get
// Author: Jordan Chandler

// Usage: lookup-get.request()

// Intentionally not using C++20 std::semaphore class for compiler compatibility reasons
//   and its unknown (to me) performance characteristics.

// Uses Fast-Semaphore class and Semaphore class as described below.
// Algorthim by Benoit Schillings, 1996-06-05, // https://www.haiku-os.org/legacy-docs/benewsletter/Issue1-26.html
// Fast-Semaphore by Joe Seigh, 2007-04,  http://www.cs.oswego.edu/pipermail/concurrency-interest/2007-April/003866.html
// C++ Implementation by Chris Thomasson, 2019-02-05 https://pastebin.com/raw/Q7p6e7Xc
// Shorten version by Martin Vorbrodt, 2019-02-05, https://vorbrodt.blog/2019/02/05/fast-semaphore/, MIT License
// Removed memory fences since atomics have internal memory barriers

#include <mutex>
#include <atomic>
#include <condition_variable>
#include <cassert>
#include <thread>
#include <iostream>
#include <curl/curl.h>
#include <string>
#include <limits>
#include <cstring>
#include <map>
#include <queue>
#include <random>
#include <algorithm>
#include <ctime>
#include <chrono>

// Classic counting semaphore class implemented using
// std::mutexes and std::condition_variables
class semaphore
{
public:
    semaphore() noexcept
    {
        semaphore(0);
    }

    semaphore(int count) noexcept
        : m_count(count) { assert(count > -1); }

    void post() noexcept
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            ++m_count;
        }
        m_cv.notify_one();
    }

    void wait() noexcept
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [&]() { return m_count != 0; });
        --m_count;
    }

private:
    int m_count;
    std::mutex m_mutex;
    std::condition_variable m_cv;
};

// Fast semaphore class implemented using atomic variables when
// possible and a contained class semaphore class when needed.
class fast_semaphore
{
public:
    fast_semaphore() noexcept
    {
        fast_semaphore(0);
    }

    fast_semaphore(int count) noexcept
        : m_count(count), m_semaphore(0) {}

    void post()
    {
        int count = m_count.fetch_add(1, std::memory_order_release);
        if (count < 0)
            m_semaphore.post();
    }

    void wait()
    {
        int count = m_count.fetch_sub(1, std::memory_order_acquire);
        if (count < 1)
            m_semaphore.wait();
    }

private:
    std::atomic<int> m_count;
    semaphore m_semaphore;
};

class lookup_get
{

public:
    lookup_get(){};

    std::map<std::string, std::string> request(
        const std::vector<std::string> &ids,
        const std::string base_url,
        const unsigned long port,
        const std::string authorization_token,
        const unsigned int max_requests)
    {
        // Load up the request slots specified by caller
        for (int i = 0; i < max_requests; i++)
        {
            request_slot.post();
        }

        // Queue the requests
        for (auto id : ids)
        {
            requests.push(id);
        }

        std::thread threads[max_requests];

        // Start workers
        for (int worker_number = 0; worker_number < max_requests; worker_number++)
        {
            threads[worker_number] = std::thread(&lookup_get::requestor, this, worker_number, base_url, port, authorization_token);
        }

        // Wait for threads to finish
        for (int worker_number = 0; worker_number < max_requests; worker_number++)
        {
            threads[worker_number].join();
        }
        return responses;
    }

private:
    // Store the requests in a shared vector
    std::queue<std::string> requests;

    // Protect access to the shared requests
    std::mutex requests_accessor;

    // Fast semaphore hold number of requests slots
    fast_semaphore request_slot;

    // Store the responses in an associative map for caching loolup
    std::map<std::string, std::string> responses;

    // Use a mutex to control access to the shared responses
    std::mutex responses_accessor;

    // Memory structure pointing to requestor threads memory for HTTP response data
    struct MemoryStruct
    {
        char *memory;
        size_t size;
    };

    // callback function to writes chunks of curl response data to the requesting thread's MemoryStruct
    static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
    {
        struct MemoryStruct *mem = (struct MemoryStruct *)userp;

        // Expand the memory buffer as needed
        size_t realsize = size * nmemb;
        char *ptr = (char *)realloc(mem->memory, mem->size + realsize + 1);
        if (ptr == NULL)
        {
            /* out of memory! */
            printf("not enough memory (realloc returned NULL)\n");
            return 0;
        }

        // Copy response content to memory bufer
        mem->memory = ptr;
        memcpy(&(mem->memory[mem->size]), contents, realsize);
        mem->size += realsize;
        mem->memory[mem->size] = 0;

        return realsize;
    }

    // requestor function runs on a thread and makes HTTP requests by:
    //  1) removing an id from the outstanding requests queue
    //  2) looking up the id in cache and returning the cached data if found
    //  3) waiting for a request_slot
    //  4) requesting the data from the web service
    //  5) caching returned data or error status codes
    void requestor(
        int worker_number,
        const std::string base_url,
        const unsigned long port,
        const std::string authorization_token)
    {

        std::string id;

        CURL *curl;
        curl = curl_easy_init();
        // Use libCUrl to make HTTP requests
        if (curl)
        {
            // Add the HTTP headers
            std::string authorization_header = "Authorization: " + authorization_token;
            struct curl_slist *list = NULL;
            list = curl_slist_append(list, "Accept: text/json");
            list = curl_slist_append(list, authorization_header.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

            struct MemoryStruct response_data;

            // Allocate memory intially with size 0.
            // The write_callback function will memory grow as needed.
            response_data.memory = (char *)malloc(1);
            response_data.size = 0;

            // Register a callback function to get the response payload
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response_data);

            // Make requests until supply is exhausted
            while (true)
            {
                // Get a new request item
                requests_accessor.lock();
                if (requests.size() == 0)
                {
                    requests_accessor.unlock();
                    break;
                }
                id = requests.front();
                requests.pop();
                requests_accessor.unlock();

                // Check for cached responses
                responses_accessor.lock();
                if (responses.find(id) == responses.end())
                {
                    // Response is not cached.
                    // The request queue can contain multiple requests for the same item_id.
                    // Reserve a spot in the response map so these duplicate requests will be ignored
                    // as soon as we commit to making the request.
                    responses[id] = "";
                    responses_accessor.unlock();
                }
                else
                {
                    // Response is cached, don't re-request
                    responses_accessor.unlock();
                    continue;
                }

                // Wait on a request slot to avoid server overrun responses
                request_slot.wait();

                // Construct the URL for the next id
                std::string current_url = base_url + id;
                curl_easy_setopt(curl, CURLOPT_URL, current_url.c_str());
                curl_easy_setopt(curl, CURLOPT_PORT, port);

                // Reallocate memory for each request
                response_data.memory = (char *)realloc(response_data.memory, 1);
                response_data.size = 0;

                // Setup an error buffer
                std::string curl_error(CURL_ERROR_SIZE, '\0');
                curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_error.c_str());

                // Make the HTTP request
                CURLcode curl_code = curl_easy_perform(curl);

                std::chrono::high_resolution_clock::time_point timestamp = std::chrono::high_resolution_clock::now();

                // Cache data if available
                long http_code = 0;
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
                if (http_code == 200 && curl_code != CURLE_ABORTED_BY_CALLBACK)
                {
                    // The update the response reservation with the response payload
                    std::string response(response_data.memory, response_data.size);
                    std::string response_string;
                    response_string.reserve(id.size() + 100);
                    response_string += "{\"id\":\"";
                    response_string += id;
                    response_string += "\"";
                    response_string += ",\"timestamp\":";
                    response_string += std::to_string(timestamp.time_since_epoch().count());
                    response_string += ",\"status\":";
                    response_string += std::to_string(http_code);
                    response_string += ",\"response\":";
                    response_string += response;
                    response_string += "}";

                    responses_accessor.lock();
                    responses[id] = response_string;
                    responses_accessor.unlock();
                }
                else if (http_code == 429)
                {
                    // The server is too busy and wants us to back off.
                    // Although we are not the cause because we control our request rate,
                    // rollback the request so it can be retried.

                    // Remove response reservation.
                    // Other workers can now fulfill request.
                    responses_accessor.lock();
                    responses.erase(id);
                    responses_accessor.unlock();

                    // Other workers may have already fulfilled a duplicate request.
                    // Even so, rollback the request removal so there is at least
                    // one request for this id in the queue.
                    requests_accessor.lock();
                    requests.push(id);
                    requests_accessor.unlock();
                }
                else
                {
                    // For any HTTP status code not handled above including 403 NOT AUITHORIZED,
                    // and 404 (NOT FOUND), update the response reservation with the status code 
                    // and null reponse payload.
                    std::string response_string;
                    response_string.reserve(id.size() + 100);
                    response_string += "{\"id\":\"";
                    response_string += id;
                    response_string += "\"";
                    response_string += ",\"timestamp\":";
                    response_string += std::to_string(timestamp.time_since_epoch().count());
                    response_string += ",\"status\":";
                    response_string += std::to_string(http_code);
                    response_string += ",\"response\":null}";
                    responses_accessor.lock();
                    responses[id] = response_string;
                    responses_accessor.unlock();
                }

                // Free the request slot so another thread can send
                request_slot.post();
            }
            // Cleanup curl objects
            free(response_data.memory);
            curl_slist_free_all(list);
            curl_easy_cleanup(curl);
        }
    }
};

// #ifdef __cplusplus
// }
// #endif

#endif /* LOOKUP_GET_CPP_INCLUDED */