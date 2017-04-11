// Copyright Doug Moen 2016-2017.
// Distributed under The MIT License.
// See accompanying file LICENSE.md or https://opensource.org/licenses/MIT

#ifndef CURV_ANALYZER_H
#define CURV_ANALYZER_H

#include <curv/meaning.h>
#include <curv/builtin.h>

namespace curv {

struct Environ
{
    Environ* parent_;
    /// eval_frame_ is nullptr, unless we are analyzing a script due to an
    /// evaluation-time call to `file`. It's used as an Exception Context,
    /// to add a stack trace to compile time errors.
    Frame* eval_frame_;
    slot_t frame_nslots_;
    slot_t frame_maxslots_;

    Environ(Environ* p)
    :
        parent_(p),
        eval_frame_(p == nullptr ? nullptr : p->eval_frame_),
        frame_nslots_(0),
        frame_maxslots_(0)
    {}
    Shared<Meaning> lookup(const Identifier& id);
    virtual Shared<Meaning> single_lookup(const Identifier&) = 0;
};

struct Builtin_Environ : public Environ
{
protected:
    const Namespace& names;
public:
    Builtin_Environ(const Namespace& n, Frame* f)
    :
        Environ(nullptr),
        names(n)
    {
        eval_frame_ = f;
    }
    virtual Shared<Meaning> single_lookup(const Identifier&);
};

/// A Definition is a partially analyzed phrase that binds a name to a value.
/// (In the future, a Definition can bind multiple names.)
///
/// There are multiple syntactic forms for definitions, and they are all
/// converted to Definition objects by Phrase::analyze_def(), which provides
/// a common interface for further analysis.
///
/// The definiens is not analyzed. In a recursive scope, we need to catalogue
/// all of the names bound within the scope before we analyze any of the
/// definientia.
struct Definition : public aux::Shared_Base
{
    Shared<const Identifier> name_;
    Shared<Phrase> definiens_;
    enum Kind { k_recursive, k_sequential } kind_;

    Definition(
        Shared<const Identifier> name,
        Shared<Phrase> definiens,
        Kind k)
    :
        name_(std::move(name)),
        definiens_(std::move(definiens)),
        kind_(k)
    {}
};

/// Analyze a set of definitions and a sequence of actions,
/// as found in a block or a module literal.
/// Currently, either all definitions are recursive or all are sequential:
/// mixing the two types is not implemented yet.
struct Bindings_Analyzer : public Environ
{
    struct Action_Phrase
    {
        int seq_no_;
        Shared<const Phrase> phrase_;

        Action_Phrase(int seq_no, Shared<const Phrase> phrase)
        : seq_no_(seq_no), phrase_(std::move(phrase))
        {}
    };
    struct Definiens
    {
        slot_t slot_;
        int seq_no_;
        Shared<const Definition> def_;

        Definiens(slot_t slot, int seq_no, Shared<const Definition> def)
        : slot_(slot), seq_no_(seq_no), def_(std::move(def))
        {}
    };
    int seq_count_ = 0;
    int seq_def_count_ = 0;
    slot_t slot_count_ = 0;
    Atom_Map<Definiens> def_dictionary_ = {};
    Shared<Module::Dictionary> defn_dictionary_;
    Module::Dictionary nonlocal_dictionary_;
    std::vector<Shared<const Phrase>> defn_phrases_;
    std::vector<Action_Phrase> action_phrases_;
    Bindings bindings_;

    // First, construct the Bindings_Analyzer:
    Bindings_Analyzer(Environ& parent)
    :
        Environ(&parent),
        defn_dictionary_(make<Module::Dictionary>()),
        nonlocal_dictionary_(),
        defn_phrases_(),
        action_phrases_(),
        bindings_()
    {
        frame_nslots_ = parent.frame_nslots_;
        frame_maxslots_ = parent.frame_maxslots_;
        bindings_.slot_ = frame_nslots_++;
        frame_maxslots_ = std::max(frame_nslots_, frame_maxslots_);
    }

    struct Thunk_Environ : public Environ
    {
        Bindings_Analyzer& ba_;
        Thunk_Environ(Bindings_Analyzer& ba)
        :
            Environ(ba.parent_),
            ba_(ba)
        {}
        virtual Shared<Meaning> single_lookup(const Identifier&);
    };

    bool is_function_definition(slot_t);
    virtual Shared<Meaning> single_lookup(const Identifier&);

    // Second, add some statements (definitions or actions):
    void add_statement(Shared<const Phrase> statement);

    // Third, analyze the statements and finalize the data members.
    void analyze(Shared<const Phrase> source);

    // Finally, move or copy the desired data members to construct a Meaning.
};

Shared<Operation> analyze_op(const Phrase& ph, Environ& env);

} // namespace
#endif // header guard
