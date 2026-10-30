#pragma once

#include "GraphAccess/GraphView.hpp"
#include "NodeTest/NodeRule.hpp"
#include <filesystem>
#include <optional>

namespace CorGIReg {

// Format-independent meaning of a definition: an OpenMath symbol, identified by
// its content dictionary and name (arith1/plus). cdbase is empty for the
// standard OpenMath CD base.
struct OpenMathSymbol {
    std::string cd, name, cdbase;
    bool operator==(const OpenMathSymbol& other) const {
        return cd == other.cd && name == other.name && cdbase == other.cdbase;
    }
};

// One semantic predicate from a node-definition JSON file (schema 1, 2 or 3).
struct NodeDefinition {
    std::uint64_t schemaVersion = 1;
    std::string name, description;
    std::string source; // Absolute definition file, for diagnostics and reports.
    // Format name -> original rule object: {"op_types": [...]} plus an optional
    // schema 2 "where" NodeTest rule.
    std::map<std::string, NodeValue> formats;
    // Schema 3 only; absent when no OpenMath symbol states the exact meaning.
    std::optional<OpenMathSymbol> openmath;

    // Validates a parsed document. Errors are prefixed with source.
    static NodeDefinition fromDocument(const NodeValue& document, std::string source);
    // Null when the definition has no mapping for the format.
    const NodeValue* rule(const std::string& format) const;
    // Exact original operation names accepted for the format; throws when unmapped.
    std::vector<std::string> operations(const std::string& format) const;
    // {"cd", "name", "cdbase"?} as written in reports, or null without a symbol.
    NodeValue openMathDocument() const;
};

// Per definition: matching source IDs, or nullopt when the selected format is unmapped.
using DefinitionMemberships = std::map<std::string, std::optional<NodeIds>>;

// Vocabulary supplied explicitly for one matching or generation invocation.
// Definitions are never added to the process-wide NodeDictionary.
class NodeLibrary {
public:
    // Files, or directories whose immediate *.json files load in sorted order.
    // Duplicate names are errors, including the same file given twice.
    static NodeLibrary load(const std::vector<std::filesystem::path>& locations);
    static NodeDefinition loadFile(const std::filesystem::path& file);

    void add(NodeDefinition definition);
    const std::map<std::string, NodeDefinition>& definitions() const noexcept { return definitions_; }
    const NodeDefinition* find(const std::string& name) const;
    bool empty() const noexcept { return definitions_.empty(); }

    // Candidates are documents whose "op" string is listed in op_types; a
    // "where" rule then filters them. Rules of every mapped definition are
    // compiled, even without candidates, so invalid rules always fail.
    DefinitionMemberships resolve(const std::string& format, const std::map<NodeId, NodeValue>& documents) const;

    // Concrete operation emitted for a definition by graph generation: the first
    // op_type. Nullopt for names outside the library. Throws for an unmapped
    // format or a conditional ("where") mapping, which generation cannot satisfy.
    std::optional<std::string> generatedOperation(const std::string& name, const std::string& format) const;

private:
    std::map<std::string, NodeDefinition> definitions_;
};

// Available memberships replace raw predicates of the same name.
void addDefinitionPredicates(NodePredicates& predicates, const DefinitionMemberships& memberships);

}
