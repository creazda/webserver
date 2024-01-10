#ifndef __SYLAR_CONFIG_H__
#define __SYLAR_CONFIG_H__
#include <memory>
#include <sstream>
#include <boost/lexical_cast.hpp>
#include <yaml-cpp/yaml.h>
#include <vector>
#include <list>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include "sylar/mutex.h"
#include "sylar/log.h"
namespace sylar
{
    class ConfigVarBase
    {
    public:
        typedef std::shared_ptr<ConfigVarBase> ptr;
        ConfigVarBase(const std::string &name, const std::string description = "") : mName(name),
                                                                                     mDescription(description)
        {
            std::transform(mName.begin(), mName.end(), mName.begin(), ::tolower);
        }
        virtual ~ConfigVarBase() {}
        const std::string &GetName() const { return mName; }
        const std::string &GetDescription() const { return mDescription; }

        virtual std::string ToString() = 0;
        virtual bool FromString(const std::string &val) = 0;
        virtual std::string GetTypeName() const = 0;

    private:
        std::string mName;
        std::string mDescription;
    };

    // F from_type  T to_type
    template <typename F, typename T>
    class LexicalCast
    {
    public:
        T operator()(const F &v)
        {
            return boost::lexical_cast<T>(v);
        }
    };

    //  vec 特例化
    template <typename T>
    class LexicalCast<std::string, std::vector<T>>
    {
    public:
        std::vector<T> operator()(const std::string &v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::vector<T> vec;
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); i++)
            {
                ss.str("");
                ss << node[i];
                vec.push_back(LexicalCast<std::string, T>()(ss.str()));
            }
            return vec;
        }
    };
    template <typename T>
    class LexicalCast<std::vector<T>, std::string>
    {
    public:
        std::string operator()(std::vector<T> &v)
        {
            YAML::Node node;
            for (auto &i : v)
            {
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i))); // 把T变成string然后变成node成员格式，放入node中
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
    // list
    template <typename T>
    class LexicalCast<std::string, std::list<T>>
    {
    public:
        std::list<T> operator()(const std::string &v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::list<T> list;
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); i++)
            {
                ss.str("");
                ss << node[i];
                list.push_back(LexicalCast<std::string, T>()(ss.str()));
            }
            return list;
        }
    };
    template <typename T>
    class LexicalCast<std::list<T>, std::string>
    {
    public:
        std::string operator()(std::list<T> &v)
        {
            YAML::Node node;
            for (auto &i : v)
            {
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i))); // 把T变成string然后变成node成员格式，放入node中
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    //  set
    template <typename T>
    class LexicalCast<std::string, std::set<T>>
    {
    public:
        std::set<T> operator()(const std::string &v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::set<T> set;
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); i++)
            {
                ss.str("");
                ss << node[i];
                set.insert(LexicalCast<std::string, T>()(ss.str()));
            }
            return set;
        }
    };
    template <typename T>
    class LexicalCast<std::set<T>, std::string>
    {
    public:
        std::string operator()(std::set<T> &v)
        {
            YAML::Node node;
            for (auto &i : v)
            {
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i))); // 把T变成string然后变成node成员格式，放入node中
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // unorder_set
    template <typename T>
    class LexicalCast<std::string, std::unordered_set<T>>
    {
    public:
        std::unordered_set<T> operator()(const std::string &v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::unordered_set<T> unordered_set;
            std::stringstream ss;
            for (size_t i = 0; i < node.size(); i++)
            {
                ss.str("");
                ss << node[i];
                unordered_set.insert(LexicalCast<std::string, T>()(ss.str()));
            }
            return unordered_set;
        }
    };
    template <typename T>
    class LexicalCast<std::unordered_set<T>, std::string>
    {
    public:
        std::string operator()(std::unordered_set<T> &v)
        {
            YAML::Node node;
            for (auto &i : v)
            {
                node.push_back(YAML::Load(LexicalCast<T, std::string>()(i))); // 把T变成string然后变成node成员格式，放入node中
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    // map
    template <typename T>
    class LexicalCast<std::string, std::map<std::string, T>>
    {
    public:
        std::map<std::string, T> operator()(const std::string &v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::map<std::string, T> map;
            std::stringstream ss;
            for (auto it = node.begin(); it != node.end(); it++)
            {
                ss.str("");
                ss << it->second;
                map.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
            }
            return map;
        }
    };
    template <typename T>
    class LexicalCast<std::map<std::string, T>, std::string>
    {
    public:
        std::string operator()(std::map<std::string, T> &v)
        {
            YAML::Node node;
            for (auto &i : v)
            {
                node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second)); // 把T变成string然后变成node成员格式，放入node中
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };
    // unordered_map
    template <typename T>
    class LexicalCast<std::string, std::unordered_map<std::string, T>>
    {
    public:
        std::unordered_map<std::string, T> operator()(const std::string &v)
        {
            YAML::Node node = YAML::Load(v);
            typename std::unordered_map<std::string, T> unordered_map;
            std::stringstream ss;
            for (auto it = node.begin(); it != node.end(); it++)
            {
                ss.str("");
                ss << it->second;
                unordered_map.insert(std::make_pair(it->first.Scalar(), LexicalCast<std::string, T>()(ss.str())));
            }
            return unordered_map;
        }
    };
    template <typename T>
    class LexicalCast<std::unordered_map<std::string, T>, std::string>
    {
    public:
        std::string operator()(std::unordered_map<std::string, T> &v)
        {
            YAML::Node node;
            for (auto &i : v)
            {
                node[i.first] = YAML::Load(LexicalCast<T, std::string>()(i.second)); // 把T变成string然后变成node成员格式，放入node中
            }
            std::stringstream ss;
            ss << node;
            return ss.str();
        }
    };

    template <typename T,
              typename FromStr = LexicalCast<std::string, T>,
              typename ToStr = LexicalCast<T, std::string>>
    class ConfigVar : public ConfigVarBase
    {
    public:
        typedef RWMutex MutexType;
        typedef std::shared_ptr<ConfigVar> ptr;
        typedef std::function<void(const T &oldValue, const T &newValue)> onChangeCB;
        ConfigVar(const std::string &name, const T &default_value, const std::string &description = "")
            : ConfigVarBase(name, description), mVal(default_value) {}

        std::string ToString() override
        {
            try
            {
                MutexType::ReadLock Lock(mMutex);
                return ToStr()(mVal);
            }
            catch (std::exception &e)
            {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::ToString exception"
                                                  << e.what() << " convert " << typeid(mVal).name() << " to string ";
            }
            return "";
        }

        bool FromString(const std::string &val) override
        {
            try
            {
                SetValue(FromStr()(val));
            }
            catch (std::exception &e)
            {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "ConfigVar::FromString exception"
                                                  << e.what() << " convert: string to " << typeid(mVal).name();
            }
            return false;
        }

        const T GetValue()
        {
            MutexType::ReadLock Lock(mMutex);
            return mVal;
        }
        // 设置value的值，相同无所谓，不同就触发各种回调函数
        void SetValue(const T &v)
        {
            {
                MutexType::ReadLock Lock(mMutex);
                if (v == mVal)
                {
                    return;
                }
                for (auto &i : mCBs)
                {
                    i.second(mVal, v);
                }
            }
            MutexType::WriteLock Lock(mMutex);
            mVal = v;
        }
        std::string GetTypeName() const override { return typeid(T).name(); }

        uint64_t AddListener(onChangeCB cb)
        {
            static uint64_t s_fun_id = 0;
            MutexType::WriteLock Lock(mMutex);
            ++s_fun_id;
            mCBs[s_fun_id] = cb;
            return s_fun_id;
        }
        void DelListener(uint64_t key)
        {
            MutexType::WriteLock Lock(mMutex);
            mCBs.erase(key);
        }
        onChangeCB GetListener(uint64_t key)
        {
            MutexType::ReadLock Lock(mMutex);
            auto it = mCBs.find(key);
            return it == mCBs.end() ? nullptr : it->second;
        }
        void ClearListener()
        {
            MutexType::WriteLock Lock(mMutex);
            mCBs.clear();
        }

    private:
        MutexType mMutex;
        T mVal;
        std::map<uint64_t, onChangeCB> mCBs; // 变更回调函数组，uint64_t唯一，方便控制func，因为function无法比较
    };

    class Config
    {
    public:
        typedef std::map<std::string, ConfigVarBase::ptr> ConfigVarMap;
        typedef RWMutex MutexType;
        template <typename T>
        static typename ConfigVar<T>::ptr Lookup(const std::string &name, const T &default_value, const std::string &description = "")
        {
            MutexType::WriteLock Lock(GetMutex());
            // 找到就判断是不是类型不同
            auto it = GetDatas().find(name);
            if (it != GetDatas().end())
            {
                auto tmp = std::dynamic_pointer_cast<ConfigVar<T>>(it->second); // 向下进行动态智能指针转换configvarbase::ptr  --- configvar<T>::ptr
                if (tmp)
                {
                    SYLAR_LOG_INFO(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists";
                    return tmp;
                }
                else
                {
                    SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name=" << name << " exists but type not " << typeid(T).name() << " real type: " << it->second->GetTypeName() << " " << it->second->ToString();
                    return nullptr;
                }
            }

            if (name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ._012345678") != std::string::npos)
            {
                SYLAR_LOG_ERROR(SYLAR_LOG_ROOT()) << "Lookup name invalid " << name;
                throw std::invalid_argument(name);
            }

            typename ConfigVar<T>::ptr v(new ConfigVar<T>(name, default_value, description));
            GetDatas()[name] = v;
            return v;
        }

        template <class T>
        static typename ConfigVar<T>::ptr Lookup(const std::string &name)
        {
            MutexType::ReadLock Lock(GetMutex());
            auto it = GetDatas().find(name);
            if (it == GetDatas().end())
            {
                return nullptr;
            }
            //
            return std::dynamic_pointer_cast<ConfigVar<T>>(it->second);
        }
        static ConfigVarBase::ptr LookupBase(const std::string &name);
        static void LoadFromYaml(const YAML::Node &root);
        static void Visit(std::function<void(ConfigVarBase::ptr)> cb);

    private:
        static ConfigVarMap &GetDatas()
        {
            static ConfigVarMap sDatas;
            return sDatas;
        }
        static MutexType &GetMutex()
        {
            static MutexType s_mutex;
            return s_mutex;
        }
    };

} // namespace syalr
#endif
