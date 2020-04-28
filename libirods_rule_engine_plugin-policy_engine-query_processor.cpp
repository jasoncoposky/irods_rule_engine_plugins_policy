
#include "policy_engine.hpp"
#include "policy_engine_parameter_capture.hpp"
#include "policy_engine_configuration_manager.hpp"

#include "irods_query.hpp"
#include "thread_pool.hpp"
#include "query_processor.hpp"

#include "json.hpp"

namespace {
    namespace pe   = irods::policy_engine;
    namespace fs   = irods::experimental::filesystem;
    namespace fsvr = irods::experimental::filesystem::server;

    namespace tokens {
            const std::string  current_time{"IRODS_TOKEN_CURRENT_TIME"};
            const std::string  lifetime{"IRODS_TOKEN_LIFETIME"};
            const std::string  collection_name{"IRODS_TOKEN_COLLECTION_NAME"};
            const std::string  data_name{"IRODS_TOKEN_DATA_NAME"};
            const std::string  source_resource_name{"IRODS_TOKEN_SOURCE_RESOURCE_NAME"};
            const std::string  destination_resource_name{"IRODS_TOKEN_DESTINATION_RESOURCE_NAME"};
    }; // tokens

    void replace_query_string_token(std::string& query_string, const std::string token, const std::string& value)
    {
        std::string::size_type pos{0};
        while(std::string::npos != pos) {
            pos = query_string.find(token);
            if(std::string::npos != pos) {
                try {
                    query_string.replace(pos, token.length(), value);
                }
                catch( const std::out_of_range& _e) {
                }
            }
        }
    } // replace_query_string_tokens

    template<typename T>
    void replace_query_string_token(std::string& query_string, const std::string token, T value)
    {
        auto value_string{std::to_string(value)};
        replace_query_string_token(query_string, token, value_string);
    } // replace_query_string_tokens

    irods::error query_processor_policy(const pe::context& ctx)
    {
        try {
            std::string user_name{}, logical_path{}, source_resource{}, destination_resource{};

            // event handler or direct call invocation
            std::tie(user_name, logical_path, source_resource, destination_resource) =
                capture_parameters(ctx.parameters, tag_first_resc);

            irods::error err;
            pe::configuration_manager cfg_mgr{ctx.instance_name, ctx.configuration};
            auto number_of_threads{extract_object_parameter<int>("number_of_threads", ctx.parameters)};
            auto query_limit{extract_object_parameter<int>("query_limit", ctx.parameters)};
            auto query_type_string{extract_object_parameter<std::string>("query_type", ctx.parameters)};
            auto query_type{irods::query<rsComm_t>::convert_string_to_query_type(query_type_string)};
            auto query_string{extract_object_parameter<std::string>("query_string", ctx.parameters)};

            auto policy_to_invoke{extract_object_parameter<std::string>("policy_to_invoke", ctx.parameters)};

            fs::path path{logical_path};

            auto& comm = *ctx.rei->rsComm;
            if(fsvr::is_collection(comm, path)) {
                replace_query_string_token(query_string, tokens::collection_name, path.string());
            }
            else {
                replace_query_string_token(query_string, tokens::collection_name, path.parent_path().string());
                replace_query_string_token(query_string, tokens::data_name, path.object_name().string());
            }

            time_t lifetime;
            std::tie(err, lifetime) = cfg_mgr.get_value("lifetime", 0);
            replace_query_string_token(query_string, tokens::lifetime, std::time(nullptr) - lifetime);
            replace_query_string_token(query_string, tokens::current_time, std::time(nullptr));

            replace_query_string_token(query_string, tokens::source_resource_name, source_resource);
            replace_query_string_token(query_string, tokens::destination_resource_name, destination_resource);

            using json       = nlohmann::json;
            using result_row = irods::query_processor<rsComm_t>::result_row;

            auto params_to_pass = ctx.parameters;

            auto job = [&](const result_row& _results) {
                auto res_arr = json::array();
                for(auto& r : _results) {
                    res_arr.push_back(r);
                }

                params_to_pass["query_results"] = res_arr;
                std::string params_str = params_to_pass.dump();

                std::string config_str{};
                if(ctx.parameters.find("configuration") !=
                   ctx.parameters.end()) {
                   config_str = ctx.parameters.at("configuration").dump();
                }

                std::list<boost::any> arguments;
                arguments.push_back(boost::any(std::ref(params_str)));
                arguments.push_back(boost::any(std::ref(config_str)));
                irods::invoke_policy(ctx.rei, policy_to_invoke, arguments);
            }; // job

            irods::thread_pool thread_pool{number_of_threads};
            irods::query_processor<rsComm_t> qp(query_string, job, query_limit, query_type);
            auto future = qp.execute(thread_pool, *ctx.rei->rsComm);
            auto errors = future.get();
            if(errors.size() > 0) {
                for(auto& e : errors) {
                    rodsLog(
                        LOG_ERROR,
                        "query failed [%d]::[%s]",
                        std::get<0>(e),
                        std::get<1>(e).c_str());
                }

                return ERROR(
                           SYS_INVALID_OPR_TYPE,
                           boost::format(
                           "query processor encountered an error for [%d] rows for query [%s]")
                           % errors.size()
                           % query_string.c_str());
            }
        }
        catch(const irods::exception& e) {
            if(CAT_NO_ROWS_FOUND == e.code()) {
                // if nothing of interest is found, thats not an error
            }
            else {
                irods::exception_to_rerror(
                    e, ctx.rei->rsComm->rError);
                return ERROR(
                          e.code(),
                          e.what());
            }
        }

        return SUCCESS();

    } // query_processor_policy

} // namespace

const char usage[] = R"(
{
    "id": "file:///var/lib/irods/configuration_schemas/v3/policy_engine_usage.json",
    "$schema": "http://json-schema.org/draft-04/schema#",
    "description": ""
        "input_interfaces": [
            {
                "name" : "event_handler-collection_modified",
                "description" : "",
                "json_schema" : ""
            },
            {
                "name" :  "event_handler-data_object_modified",
                "description" : "",
                "json_schema" : ""
            },
            {
                "name" :  "event_handler-metadata_modified",
                "description" : "",
                "json_schema" : ""
            },
            {
                "name" :  "event_handler-user_modified",
                "description" : "",
                "json_schema" : ""
            },
            {
                "name" :  "event_handler-resource_modified",
                "description" : "",
                "json_schema" : ""
            },
            {
                "name" :  "direct_invocation",
                "description" : "",
                "json_schema" : ""
            },
            {
                "name" :  "query_results"
                "description" : "",
                "json_schema" : ""
            },
        ],
    "output_json_for_validation" : ""
}
)";

extern "C"
pe::plugin_pointer_type plugin_factory(
      const std::string& _plugin_name
    , const std::string&)
{
    return pe::make(
                 _plugin_name
               , "irods_policy_query_processor"
               , usage
               , query_processor_policy);
} // plugin_factory
