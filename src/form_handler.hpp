#ifndef FORM_HANDLER_HPP
#define FORM_HANDLER_HPP

#include "http_request.hpp"
#include "http_response.hpp"
#include <string>
#include <unordered_map>

class FormHandler {
public:
    explicit FormHandler(const std::string& storageDir);

    HttpResponse handleForm(const HttpRequest& request, int clientFd);

private:
    std::string storageDir_;

    std::unordered_map<std::string, std::string> parseFormData(const std::string& body);

    std::string sanitise(const std::string& input);

    bool saveFormData(const std::unordered_map<std::string, std::string>& data);

    std::string generateFilename();

    
    std::string urlDecode(const std::string& encoded);
};

#endif 