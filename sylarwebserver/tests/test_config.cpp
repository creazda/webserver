#include "../sylar/config.h"
#include "../sylar/log.h"
#include <yaml-cpp/yaml.h>

sylar::ConfigVar<int>::ptr g_int_value_config = sylar::Config::Lookup("system.port", (int)8080, "system port");
// sylar::ConfigVar<float>::ptr g_intx_value_config = sylar::Config::Lookup("system.port", (float)8080, "system port");

sylar::ConfigVar<float>::ptr g_float_value_config = sylar::Config::Lookup("system.value", (float)12.31f, "system value");
sylar::ConfigVar<std::vector<int>>::ptr g_int_vec_value_config = sylar::Config::Lookup("system.int_vec", std::vector<int>{1, 2}, "system int_vec");
sylar::ConfigVar<std::list<int>>::ptr g_int_list_value_config = sylar::Config::Lookup("system.int_list", std::list<int>{1, 2}, "system int_list");
sylar::ConfigVar<std::set<int>>::ptr g_int_set_value_config = sylar::Config::Lookup("system.int_set", std::set<int>{1, 2}, "system int_set");
sylar::ConfigVar<std::unordered_set<int>>::ptr g_int_uset_value_config = sylar::Config::Lookup("system.int_uset", std::unordered_set<int>{1, 2}, "system int_uset");
sylar::ConfigVar<std::map<std::string, int>>::ptr g_str_int_map_value_config = sylar::Config::Lookup("system.str_int_map", std::map<std::string, int>{{"k", 2}}, "system str_int_map");
sylar::ConfigVar<std::unordered_map<std::string, int>>::ptr g_str_int_umap_value_config = sylar::Config::Lookup("system.str_int_umap", std::unordered_map<std::string, int>{{"k", 2}}, "system str_int_umap");

// node中kv格式的用it便利，数组格式的用for循环遍历
void print_yaml(const YAML::Node &node, int level)
{
    if (node.IsScalar())
    {
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ')
                                         << node.Scalar() << " - " << node.Type() << " - " << level << "       isscalar";
    }
    else if (node.IsMap())
    {
        for (auto it = node.begin(); it != node.end(); it++)
        {
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ')
                                             << it->first << " - " << it->second.Type() << " - " << level << "       ismap";
            print_yaml(it->second, level + 1);
        }
    }
    else if (node.IsSequence())
    {
        for (size_t i = 0; i < node.size(); i++)
        {
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << std::string(level * 4, ' ')
                                             << i << " - " << node[i].Type() << " - " << level << "       issequence";
            print_yaml(node[i], level + 1);
        }
    }
}

void test_yaml()
{
    YAML::Node root = YAML::LoadFile("/home/dcy/work/sylarwebserver/bin/conf/test.yml");
    print_yaml(root, 0);
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << root.Scalar();
}
void test_config()
{
    // SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before: " << g_int_value_config->GetValue();
    // SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before: " << g_float_value_config->ToString();

#define XX(g_var, name, prefix)                                                               \
    {                                                                                         \
        auto &v = g_var->GetValue();                                                          \
        for (auto &i : v)                                                                     \
        {                                                                                     \
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix " " #name ": " << i;                  \
        }                                                                                     \
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix " " #name " yaml: " << g_var->ToString(); \
    }
#define XX_M(g_var, name, prefix)                                                                               \
    {                                                                                                           \
        auto &v = g_var->GetValue();                                                                            \
        for (auto &i : v)                                                                                       \
        {                                                                                                       \
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix " " #name ": {" << i.first << " - " << i.second << "}"; \
        }                                                                                                       \
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << #prefix " " #name " yaml: " << g_var->ToString();                   \
    }

    XX(g_int_vec_value_config, int_vec, before);
    XX(g_int_list_value_config, int_list, before);
    XX(g_int_set_value_config, int_set, before);
    XX(g_int_uset_value_config, int_uset, before);
    XX_M(g_str_int_map_value_config, str_int_map, before);
    XX_M(g_str_int_umap_value_config, str_int_umap, before);

    YAML::Node root = YAML::LoadFile("/home/dcy/work/sylarwebserver/bin/conf/test.yml");
    sylar::Config::LoadFromYaml(root);
    // SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after: " << g_int_value_config->GetValue();   // port
    // SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after: " << g_float_value_config->ToString(); // value

    XX(g_int_vec_value_config, int_vec, after);
    XX(g_int_list_value_config, int_list, after);
    XX(g_int_set_value_config, int_set, after);
    XX(g_int_uset_value_config, int_uset, after);
    XX_M(g_str_int_map_value_config, str_int_map, after);
    XX_M(g_str_int_umap_value_config, str_int_umap, after);
}

class Person
{
public:
    std::string mName;
    int mAge = 0;
    bool mSex = 0;

    std::string toString() const
    {
        std::stringstream ss;
        ss << "[Person name=" << mName
           << " age=" << mAge
           << " sex=" << mSex
           << "]";
        return ss.str();
    }
    bool operator==(const Person &oth) const
    {
        return mName == oth.mName && mAge == oth.mAge && mSex == oth.mSex;
    }
};
namespace sylar
{
    template <>
    class LexicalCast<std::string, Person>
    {
    public:
        Person operator()(const std::string &v)
        {
            YAML::Node node = YAML::Load(v);
            Person p;
            p.mName = node["name"].as<std::string>();
            p.mAge = node["age"].as<int>();
            p.mSex = node["sex"].as<bool>();
            return p;
        }
    };
    template <>
    class LexicalCast<Person, std::string>
    {
    public:
        std::string operator()(const Person &p)
        {
            YAML::Node node;
            node["name"] = p.mName;
            node["age"] = p.mAge;
            node["sex"] = p.mSex;
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
}
sylar::ConfigVar<Person>::ptr g_person = sylar::Config::Lookup("class.person", Person(), "system person");
sylar::ConfigVar<std::map<std::string, Person>>::ptr g_person_map = sylar::Config::Lookup("class.map", std::map<std::string, Person>(), "system map person");
void test_class()
{
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "before: " << g_person->GetValue().toString() << " - " << g_person->ToString();

#define XX_PM(g_var, prefix)                                                                               \
    {                                                                                                      \
        auto m = g_var->GetValue();                                                                        \
        for (auto i : m)                                                                                   \
        {                                                                                                  \
            SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << prefix << ": " << i.first << " - " << i.second.toString(); \
        }                                                                                                  \
        SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << prefix << ": size=" << m.size();                               \
    }

    g_person->AddListener([](const Person &oldValue, const Person &newValue)
                          { SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "oldValue=" << oldValue.toString() << " newValue=" << newValue.toString(); });

    XX_PM(g_person_map, "class.map before");

    YAML::Node root = YAML::LoadFile("/home/dcy/work/sylarwebserver/bin/conf/test.yml");
    sylar::Config::LoadFromYaml(root);
    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "after: " << g_person->GetValue().toString() << " - " << g_person->ToString();
    XX_PM(g_person_map, "class.map after");
}
void test_log()
{
    static sylar::Logger::ptr system_log = SYLAR_LOG_NAME("system");
    SYLAR_LOG_INFO(system_log) << "hello system";
    std::cout << "before: " << sylar::LoggerMgr::GetInstance()->ToYamlString() << std::endl;
    YAML::Node root = YAML::LoadFile("/home/dcy/work/sylarwebserver/bin/conf/log.yml");
    sylar::Config::LoadFromYaml(root);
    std::cout << "after: " << sylar::LoggerMgr::GetInstance()->ToYamlString() << std::endl;
    // std::cout << sylar::LoggerMgr::GetInstance()->ToYamlString() << std::endl;
    SYLAR_LOG_INFO(system_log) << "hello system";
    system_log->SetFormatter("%d - %m%n");
    SYLAR_LOG_INFO(system_log) << "hello system";
}

int main(int argc, char **argv)
{
    // test_yaml();
    // test_config();
    // test_class();
    // test_log();
    sylar::Config::Visit([](sylar::ConfigVarBase::ptr var)
                         { SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "name= " << var->GetName()
                                                            << " description= " << var->GetDescription()
                                                            << " typename= " << var->GetTypeName()
                                                            << " value= " << var->ToString(); });
    return 0;
}