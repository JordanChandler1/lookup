#include <random>
#include <string>
#include <algorithm>
#include <map>
#include <iostream>
#include <limits>
#include <queue>
#include <vector>

#include "lookup_get.cpp"

std::string random_string()
{
    std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

    std::random_device rd;
    std::mt19937 generator(rd());

    std::shuffle(str.begin(), str.end(), generator);

    return str.substr(0, 32); // assumes 32 < number of characters in str
}

template <class T>
bool is_counting(T number)
{
    if ((number > 0) && (number <= std::numeric_limits<T>::max()))
    {
        return true;
    }
    return false;
}

void print_error(std::string message)
{
    std::cout << "ERROR: "
              << message
              << std::endl;
}

// Input confirmation message displayed after input is verified
void print_input(
    const std::string url, 
    unsigned long port, 
    std::string authorization_token,
    unsigned int request_count, 
    unsigned int limit)
{
    std::cout << "lookup-client"
              << " -Url "
              << url
              << " -Port"
              << port
              << " -Authorization"
              << authorization_token
              << " -Requests"
              << request_count
              << " -Limit "
              << limit
              << "\n"
              << std::endl;
}

// Usage message displayed on -h
void print_usage()
{
    std::cout
        << "lookup_client -Url <url> [-Port port] [-Authorization token] [-Requests count] [-Limit limit]"
        << std::endl
        << std::endl
        << "Items not enclosed enclosed in <> are required.  Items enclosed in [] are optional."
        << "If optional switches are not provided the following defaults are used:" << std::endl
        << "    [port]:   8080" << std::endl
        << "    [token]:" << std::endl
        << "    [count]:  100" << std::endl
        << "    [limit]:  5" << std::endl
        << std::endl
        << "Notes:" << std::endl
        << "  Switches may be abbreviated using the first letter of the switch." << std::endl
        << "  Switches may be any combination of upper case and lower case letters." << std::endl
        << "  Switches may omitted and the value will be determined positionally." << std::endl
        << "  Switches may be reordered and any value without a switch will be used to fulfill a remaining positional value." << std::endl;
}

int main(int argc, char *args[])
{
    std::string base_url = "http://localhost/items/";
    unsigned long port = 8080;
    std::string authorization_token = "";
    unsigned int limit = 5;
    unsigned int request_count = 100;

    std::vector<char> switch_letters = {'u', 'p', 'a', 'r', 'l', 'h'};
    std::reverse(switch_letters.begin(), switch_letters.end());

    std::map<char, int> switch_values = {{'u', 1}, {'p', 1}, {'a', 1}, {'r', 1}, {'l', 1}, {'h', 0}};

    char switch_letter = '\0';

    std::string arg = "";
    std::string next_arg = "";
    int i = 1;
    while ((i < argc) && (switch_letters.size() > 0))
    {
        arg = args[i];
        std::vector<std::string> values;
        if (arg.size() >= 2 && arg.at(0) == '-')
        {
            switch_letter = std::tolower((arg.size() >= 2) ? arg.at(1) : '\0');

            // Check for valid switch
            auto it = std::find(switch_letters.begin(), switch_letters.end(), switch_letter);
            if (it == switch_letters.end())
            {
                // Not valid switch letter
                std::cout << "ERROR Invalid or already used switch character: [-"
                          << switch_letter << "] or positional value." << std::endl 
                          << "If using positional values make sure they are in the rigjt order." << std::endl;
                return EXIT_FAILURE;
            }
            // Consume the switch letter
            switch_letters.erase(it, it + 1);
            i++;

            // Consume the values for this switch
            auto arg_it = switch_values.find(switch_letter);
            int expected_count = (arg_it == switch_values.end()) ? 0 : arg_it->second;
            while ((values.size() < expected_count) && ((((i < argc) ? args[i] : "-")[0]) != '-'))
            {
                std::string value = args[i];
                values.push_back(value);
                i++;
            }
            // Check for valid number of values for this switch
            if (expected_count != values.size())
            {
                // ERROR Invalid number of argument.
                std::cout << "ERROR Switch: ["
                          << switch_letter << "] had "
                          << values.size()
                          << " arguments but was expecting "
                          << expected_count
                          << "."
                          << std::endl;

                return EXIT_FAILURE;
            }
        }
        else
        {
            // Consume the next switch letter
            if (switch_letters.size() > 0)
            {
                switch_letter = switch_letters.back();
                switch_letters.pop_back();
            }
            i++;
            auto arg_it2 = switch_values.find(switch_letter);
            int expected_count = (arg_it2 == switch_values.end()) ? 0 : arg_it2->second;
            if (expected_count == 0)
            {
                // No switch associated with this positional argument
                std::cout << "ERROR Positional argument ["
                          << arg
                          << "] was unexpected."
                          << std::endl;
                return EXIT_FAILURE;
            }
            // Use the argument as the switch value
            values.push_back(arg);
        }

        char *end = nullptr;
        long number;
        switch (switch_letter)
        {
        case 'u':
            base_url = values[0];
            break;
        case 'p':
            number = strtol(values[0].c_str(), &end, 10);
            if (false)
            {
                std::cout << "ERROR The value for switch: [-p] could not be converted to a number."
                          << std::endl;
                return EXIT_FAILURE;
            }
            if (!is_counting<long>(number) || (number > 65535))
            {
                std::cout << "ERROR The value for switch: [-p] was not a valid positive number between 1 and 65535."
                          << std::endl;
                return EXIT_FAILURE;
            }
            port = number;
            break;
        case 'a':
            authorization_token = values[0];
            break;
        case 'r':
            if (false)
            {
                std::cout << "ERROR The value for switch: [-r] could not be converted to a number."
                          << std::endl;
                return EXIT_FAILURE;
            }
            number = strtol(values[0].c_str(), &end, 10);
            if (!is_counting<int>(number))
            {
                std::cout << "ERROR The value for switch: [-r] was not a valid non-zero positive number."
                          << std::endl;
                return EXIT_FAILURE;
            }
            request_count = number;
            break;
        case 'l':
            if (false)
            {
                std::cout << "ERROR The value for switch: [-l] could not be converted to a number."
                          << std::endl;
                return EXIT_FAILURE;
            }
            number = strtol(values[0].c_str(), &end, 10);
            if (!is_counting<int>(number))
            {
                std::cout << "ERROR The value for switch: [-l] was not a valid non-zero positive number."
                          << std::endl;
                return EXIT_FAILURE;
            }
            limit = number;
            break;
        case 'h':
            print_usage();
            break;
        }
        values.clear();
    }

    if (std::find(switch_letters.begin(), switch_letters.end(), 'u') != switch_letters.end())
    {
        // ERROR URL IS REQUIRED
        return EXIT_FAILURE;
    };

    print_input(base_url, port, authorization_token, request_count, limit);

    // Simulate a batch of requests
    std::vector<std::string> requests_1;
    for (int i = 0; i < request_count; i++)
    {
        std::string id = random_string();
        requests_1.push_back(id);
        // Simulate a closely space duplicate request
        requests_1.push_back(id);
    }
    // Simulate non-closely spaced duplicate requests by appending to sets of requests
    std::vector<std::string> requests;
    requests.insert(requests.end(),requests_1.begin(), requests_1.end());
    requests.insert(requests.end(),requests_1.begin(), requests_1.end());

    // Issue the requests
    lookup_get *get = new lookup_get();
    std::map<std::string, std::string> responses = get->request(requests, base_url, port, authorization_token, limit);

    // Display the results
    for (auto response : responses)
    {
        std::cout << response.second << std::endl;
    }

    return EXIT_SUCCESS;
}