#pragma once

#include "Environment.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <vector>

// The history as live bindings, the way QML binds properties. Each entry
// is a result named `$` and its number; `x = expr` binds x to expr and
// `f(x) = body` defines f, the latest entry for a name winning over
// earlier ones, wherever they are. An entry depends on every name its
// evaluation read, which the environment reports, and when an entry
// changes, everything that depends on what it defines is evaluated
// again; the rest keeps its value, a random number included. Names that
// end up depending on themselves are a binding loop, an error on each
// entry in the loop.
class Worksheet : public BindingScope
{
public:
    struct Entry
    {
        enum class State { Dirty, Evaluating, Clean };

        int id = 0;
        QString source;
        // From the source alone: the variables, or the function, the entry
        // defines, known even when it does not evaluate.
        QStringList names;
        bool definesFunction = false;

        Number value = 0;
        QString error;
        int errorPosition = -1;
        QString definition; // `f(x) = body` as shown, for a definition
        UserFunction function;
        // Bumped whenever an update changes the result, so the display can
        // show which entries followed a change.
        int revision = 0;

        QSet<QString> dependencies;
        State state = State::Dirty;
        QString loopMessage;

        QString name() const { return u'$' + QString::number(id); }
    };

    explicit Worksheet(Environment *env);
    ~Worksheet() override;

    const std::vector<Entry> &entries() const { return m_entries; }
    int size() const { return int(m_entries.size()); }
    int indexOfId(int id) const { return m_indexOfId.value(id, -1); }

    // Each of these evaluates whatever the change affects.
    int append(const QString &source);
    void edit(int index, const QString &source);
    void remove(int index);
    void clear();
    // A saved history: (id, source) pairs, ids kept.
    void load(const QList<std::pair<int, QString>> &entries);
    // The environment changed these names behind the history's back: a
    // script set a variable, the angle unit changed ("#angle").
    void namesChanged(const QSet<QString> &names);

    // The entry the name belongs to now, or -1.
    int ownerIndex(const QString &name) const { return m_owners.value(name, -1); }
    // What entry `index` binds its names to: "a * 2" of "x = a * 2".
    QString valueSource(int index) const;
    // The entry's source without `name` among the names it assigns:
    // "x = a * 2" becomes "a * 2", "x = y = 2" without y "x = 2".
    QString sourceWithout(int index, const QString &name) const;
    // The names a source refers to, each once: not those it assigns at its
    // start, nor in a definition the parameters, which hide a variable.
    static QSet<QString> referencedNames(const QString &source);
    // The source with every reference to `name` written as `text`, in
    // parentheses where it would otherwise run into what is next to it.
    static QString replaceReferences(const QString &source, const QString &name, const QString &text);

    // The indexes of the entries assigning or defining `name`.
    QList<int> definersOf(const QString &name) const;

    // The names the history binds, variables and functions.
    QStringList variableNames() const;
    QStringList functionNames() const;

    // BindingScope
    void note(const QString &name) override;
    bool owns(const QString &name) const override;
    bool isVariable(const QString &name) const override;
    bool isFunction(const QString &name) const override;
    Number variable(const QString &name, int position) override;
    const UserFunction &function(const QString &name, int position) override;
    QString owner(const QString &name) const override;
    bool isEvaluating() const override { return !m_stack.isEmpty(); }

private:
    static void analyze(Entry &entry);
    static bool canBind(const QString &name);
    void reindex();
    QHash<QString, int> ownerIds() const;
    QSet<QString> rebind(const QHash<QString, int> &before);
    QString label(int index) const;
    QSet<QString> outputs(const Entry &entry) const;
    void update(QSet<QString> changed, QSet<int> dirty, int fresh = -1);
    void evaluate(int index);
    void ensureEvaluated(int index, int position);
    Number answer(int position);
    void checkTargets(const Statement &statement, int index) const;

    Environment *m_env;
    std::vector<Entry> m_entries;
    QHash<QString, int> m_owners; // name -> index of the entry defining it
    QHash<int, int> m_indexOfId;
    QList<int> m_stack; // the entries being evaluated, innermost last
    int m_nextId = 1;
};
