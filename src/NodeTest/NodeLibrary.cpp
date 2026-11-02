#include "NodeTest/NodeLibrary.hpp"
#include "NodeTest/NodeValueJson.hpp"
#include "CompileTool/Common/NodePattern.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>

namespace CorGIReg {
namespace {
template<class T> const T* as(const NodeValue& value) { return std::get_if<T>(&value.data); }

bool predicateName(const std::string& name) {
    return !name.empty() && name.find_first_not_of(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_0123456789") == std::string::npos;
}

// OpenMath CD and symbol names: [A-Za-z][A-Za-z0-9_]*.
bool openMathName(const std::string& name) {
    return predicateName(name) && std::isalpha(static_cast<unsigned char>(name.front()));
}

OpenMathSymbol openMathSymbol(const NodeValue& value, const std::function<void(const std::string&)>& invalid) {
    const auto* fields = as<NodeValue::Object>(value);
    if (!fields) invalid("openmath must be an object");
    for (const auto& entry : *fields)
        if (entry.first != "cd" && entry.first != "name" && entry.first != "cdbase")
            invalid("Only cd, name and cdbase are supported in openmath");
    auto text = [&](const char* key) -> const std::string* {
        auto found = fields->find(key);
        return found == fields->end() ? nullptr : as<std::string>(found->second);
    };
    OpenMathSymbol symbol;
    if (!text("cd") || !openMathName(*text("cd"))) invalid("openmath cd must match [A-Za-z][A-Za-z0-9_]*");
    if (!text("name") || !openMathName(*text("name"))) invalid("openmath name must match [A-Za-z][A-Za-z0-9_]*");
    symbol.cd = *text("cd");
    symbol.name = *text("name");
    if (fields->count("cdbase")) {
        if (!text("cdbase") || text("cdbase")->empty()) invalid("openmath cdbase must be a nonempty string");
        symbol.cdbase = *text("cdbase");
    }
    return symbol;
}
}

NodeDefinition NodeDefinition::fromDocument(const NodeValue& document, std::string source) {
    NodeDefinition result;
    result.source = std::move(source);
    auto invalid = [&](const std::string& message) { throw std::invalid_argument(result.source + ": " + message); };
    const auto* object = as<NodeValue::Object>(document);
    if (!object) invalid("Invalid node definition fields");
    for (const auto& entry : *object)
        if (entry.first != "schema_version" && entry.first != "name" &&
            entry.first != "description" && entry.first != "formats" && entry.first != "openmath" &&
            entry.first != "key_matching")
            invalid("Invalid node definition fields");
    auto field = [&](const char* key) -> const NodeValue* {
        auto found = object->find(key);
        return found == object->end() ? nullptr : &found->second;
    };
    const auto* version = field("schema_version") ? as<std::uint64_t>(*field("schema_version")) : nullptr;
    if (!version || *version < 1 || *version > 4) invalid("Unsupported node definition schema");
    result.schemaVersion = *version;
    const auto* name = field("name") ? as<std::string>(*field("name")) : nullptr;
    if (!name || !predicateName(*name)) invalid("Invalid predicate name");
    result.name = *name;
    if (const auto* description = field("description")) {
        if (!as<std::string>(*description)) invalid("Invalid description for " + result.name);
        result.description = *as<std::string>(*description);
    }
    if (const auto* openmath = field("openmath")) {
        if (result.schemaVersion < 3) invalid("openmath requires schema_version 3 for " + result.name);
        result.openmath = openMathSymbol(*openmath, [&](const std::string& message) {
            invalid(message + " for " + result.name);
        });
    }
    if (const auto* matching = field("key_matching")) {
        if (result.schemaVersion < 4) invalid("key_matching requires schema_version 4");
        const auto* keys = as<NodeValue::Object>(*matching);
        if (!keys) invalid("key_matching must be an object");
        std::map<std::string, std::string> aliases;
        for (const auto& [key, mappings] : *keys) {
            if (!predicateName(key)) invalid("Invalid canonical key: " + key);
            const auto* entries = as<NodeValue::Object>(mappings);
            if (!entries) invalid("key_matching entries must be format objects: " + key);
            auto alias = [&](const std::string& name) {
                if (!predicateName(name)) invalid("Invalid key alias: " + name);
                auto found = aliases.emplace(name, key);
                if (!found.second && found.first->second != key) invalid("Ambiguous key alias: " + name);
            };
            alias(key);
            for (const auto& [format, path] : *entries) {
                if (format.empty()) invalid("Empty key mapping format for " + key);
                if (const auto* name = as<std::string>(path)) alias(*name);
                else {
                    const auto* segments = as<NodeValue::Array>(path);
                    if (!segments || segments->empty()) invalid("Key mapping must be an attribute name or nonempty path: " + key);
                    try { compileNodeRule(NodeValue::Object{{"path", path}, {"exists", true}}); }
                    catch (const std::invalid_argument& error) { invalid(error.what()); }
                    if (const auto* name = as<std::string>(segments->back())) alias(*name);
                }
            }
        }
        result.keyMatching = *keys;
    }
    const auto* formats = field("formats") ? as<NodeValue::Object>(*field("formats")) : nullptr;
    if (!formats || formats->empty()) invalid("Node " + result.name + " requires format mappings");
    for (const auto& [format, rule] : *formats) {
        const auto where = result.name + "/" + format;
        if (format.empty()) invalid("Invalid format name for " + result.name);
        const auto* fields = as<NodeValue::Object>(rule);
        const bool conditional = result.schemaVersion >= 2;
        bool valid = fields && fields->count("op_types");
        if (fields)
            for (const auto& entry : *fields)
                valid &= entry.first == "op_types" || (conditional && entry.first == "where");
        if (!valid)
            invalid(std::string(conditional ? "Only op_types and where" : "Only op_types") +
                    " rules are supported for " + where);
        if (fields->count("where") && !as<NodeValue::Object>(fields->at("where")))
            invalid("where must be a rule object: " + where);
        const auto* types = as<NodeValue::Array>(fields->at("op_types"));
        bool strings = types && !types->empty();
        if (types)
            for (const auto& type : *types) strings &= as<std::string>(type) && !as<std::string>(type)->empty();
        if (!strings) invalid("op_types must be a nonempty array of strings for " + where);
        result.formats.emplace(format, rule);
    }
    return result;
}

NodeValue::Array NodeDefinition::keyPath(const std::string& key, const std::string& format) const {
    auto path = [](const NodeValue& value) -> NodeValue::Array {
        if (const auto* name = as<std::string>(value)) return {"attributes", *name};
        return std::get<NodeValue::Array>(value.data);
    };
    for (const auto& [canonical, mappings] : keyMatching) {
        const auto& entries = std::get<NodeValue::Object>(mappings.data);
        bool matches = key == canonical;
        for (const auto& [source, value] : entries) {
            const auto parts = path(value);
            const auto* alias = as<std::string>(parts.back());
            matches |= alias && *alias == key;
        }
        if (!matches) continue;
        auto selected = entries.find(format);
        if (selected != entries.end()) return path(selected->second);
        auto json = entries.find("json");
        return json == entries.end() ? NodeValue::Array{"attributes", canonical} : path(json->second);
    }
    return {"attributes", key};
}

const NodeValue* NodeDefinition::rule(const std::string& format) const {
    auto found = formats.find(format);
    return found == formats.end() ? nullptr : &found->second;
}

std::vector<std::string> NodeDefinition::operations(const std::string& format) const {
    const auto* mapping = rule(format);
    if (!mapping) throw std::invalid_argument("No definition for predicate '" + name + "' in format " + format);
    std::vector<std::string> result;
    for (const auto& type : std::get<NodeValue::Array>(std::get<NodeValue::Object>(mapping->data).at("op_types").data))
        result.push_back(std::get<std::string>(type.data));
    return result;
}

NodeValue NodeDefinition::openMathDocument() const {
    if (!openmath) return NodeValue(nullptr);
    NodeValue::Object symbol{{"cd", openmath->cd}, {"name", openmath->name}};
    if (!openmath->cdbase.empty()) symbol.emplace("cdbase", openmath->cdbase);
    return NodeValue(std::move(symbol));
}

NodeDefinition NodeLibrary::loadFile(const std::filesystem::path& file) {
    std::ifstream input(file, std::ios::binary);
    if (!input) throw std::invalid_argument("Cannot read node definition: " + file.string());
    std::ostringstream text;
    text << input.rdbuf();
    const auto source = std::filesystem::canonical(file).string();
    NodeValue document;
    try { document = parseJson(text.str()); }
    catch (const std::invalid_argument& error) { throw std::invalid_argument(source + ": " + error.what()); }
    return NodeDefinition::fromDocument(document, source);
}

NodeLibrary NodeLibrary::load(const std::vector<std::filesystem::path>& locations) {
    NodeLibrary library;
    for (const auto& location : locations) {
        std::vector<std::filesystem::path> files;
        if (std::filesystem::is_directory(location)) {
            for (const auto& entry : std::filesystem::directory_iterator(location))
                if (entry.path().extension() == ".json" && !entry.is_directory()) files.push_back(entry.path());
            std::sort(files.begin(), files.end());
            if (files.empty()) throw std::invalid_argument("No node definitions found in " + location.string());
        } else {
            auto extension = location.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension != ".json") throw std::invalid_argument("Node definitions must be JSON files: " + location.string());
            files.push_back(location);
        }
        for (const auto& file : files) library.add(loadFile(file));
    }
    return library;
}

void NodeLibrary::add(NodeDefinition definition) {
    auto found = definitions_.find(definition.name);
    if (found != definitions_.end())
        throw std::invalid_argument("Duplicate node definition: " + definition.name + " (" +
                                    found->second.source + ", " + definition.source + ")");
    const auto name = definition.name;
    definitions_.emplace(name, std::move(definition));
}

const NodeDefinition* NodeLibrary::find(const std::string& name) const {
    auto found = definitions_.find(name);
    return found == definitions_.end() ? nullptr : &found->second;
}

DefinitionMemberships NodeLibrary::resolve(const std::string& format, const std::map<NodeId, NodeValue>& documents) const {
    DefinitionMemberships result;
    for (const auto& [name, definition] : definitions_) {
        const auto* mapping = definition.rule(format);
        if (!mapping) { result.emplace(name, std::nullopt); continue; }
        const auto& fields = std::get<NodeValue::Object>(mapping->data);
        NodeRule check;
        if (auto where = fields.find("where"); where != fields.end()) {
            try { check = compileNodeRule(where->second); }
            catch (const std::invalid_argument& error) {
                throw std::invalid_argument(definition.source + " (" + name + "): " + error.what());
            }
        }
        const auto types = definition.operations(format);
        NodeIds ids;
        for (const auto& [id, document] : documents) {
            const auto* object = as<NodeValue::Object>(document);
            const auto op = object ? object->find("op") : NodeValue::Object::const_iterator{};
            if (!object || op == object->end() || !as<std::string>(op->second)) continue;
            if (std::find(types.begin(), types.end(), *as<std::string>(op->second)) == types.end()) continue;
            if (!check || check(document)) ids.insert(id);
        }
        result.emplace(name, std::move(ids));
    }
    return result;
}

std::optional<std::string> NodeLibrary::generatedOperation(const std::string& name, const std::string& format) const {
    const auto* definition = find(name);
    if (!definition) return std::nullopt;
    const auto operations = definition->operations(format);
    if (std::get<NodeValue::Object>(definition->rule(format)->data).count("where"))
        throw std::invalid_argument(definition->source + " (" + name +
                                    "): conditional definitions cannot be generated for format " + format);
    return operations.front();
}

void addDefinitionPredicates(NodePredicates& predicates, const DefinitionMemberships& memberships) {
    for (const auto& [name, ids] : memberships)
        if (ids) predicates[name] = [ids = *ids](NodeId id) { return ids.count(id) != 0; };
}

void addInlineNodePredicates(NodePredicates& predicates, const std::set<std::string>& keys,
    const std::map<NodeId, NodeValue>& documents, const NodeLibrary* library, const std::string& format) {
    for (const auto& key : keys) {
        if (key.empty()) continue;
        const auto node = parseNodePattern(key);
        if (node.conditions.empty()) continue;
        const auto* definition = library ? library->find(node.name) : nullptr;
        if (definition && !definition->rule(format))
            throw std::invalid_argument("No definition for predicate '" + node.name + "' in format " + format);
        const auto base = predicates.find(node.name);
        NodeValue::Array conditions;
        for (const auto& [name, value] : node.conditions) {
            auto path = definition ? definition->keyPath(name, format) : NodeValue::Array{"attributes", name};
            conditions.emplace_back(NodeValue::Object{{"path", std::move(path)}, {"eq", value}});
        }
        const auto check = compileNodeRule(NodeValue::Object{{"all", std::move(conditions)}});
        NodeIds ids;
        for (const auto& [id, document] : documents)
            if (base != predicates.end() && base->second(id) && check(document)) ids.insert(id);
        predicates[key] = [ids = std::move(ids)](NodeId id) { return ids.count(id) != 0; };
    }
}

}
