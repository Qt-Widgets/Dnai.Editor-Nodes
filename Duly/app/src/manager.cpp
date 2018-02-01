#include "manager.h"
#include <QJsonDocument>
#include <QDebug>
#include <QDir>
#include <QUrl>
#include "dulyapp.h"
#include "views/declarationcanvas.h"
#include "models/treeitem.h"
#include "models/imodel.h"

namespace duly_gui {
    const QString Manager::project_extension = ".dulyproject";

    Manager::Manager(QObject *parent): QObject(parent)
    {
        m_projectModel = nullptr;
        m_declRef = new DeclarationModel();
    }

    Manager::~Manager()
    {

    }

    void Manager::createProject(const QString &name, const QString &description, const QString &path)
    {
        auto fileUrl = QUrl(QDir(path).filePath(name) + project_extension);
        QFile file(fileUrl.toLocalFile());
        if (!file.open(QIODevice::ReadWrite)) {
            qWarning("Couldn't open file.");
            qDebug() << file.errorString();
            return;
        }

        m_project = new Project(name, description, file);
        m_project->save();
    }

    QJsonObject Manager::loadProjectData(const QString &path)
    {
        Project *project = this->loadProject(path);
        return (project != nullptr) ? project->data() : QJsonObject {};
    }

    Project * Manager::loadProject(const QString &path)
    {
        QFile file(QUrl(path).toLocalFile());

        if (!file.open(QIODevice::ReadWrite)) {
            qWarning("Couldn't open file.");
            return nullptr;
        }

        QByteArray data = file.readAll();

        QJsonObject obj(QJsonDocument::fromJson(data).object());

        Project *project = new Project(obj["name"].toString(), obj["description"].toString(), file);
        project->unserialize(obj);
        return project;
    }

    void Manager::openProject(const QString &path)
    {
        this->openProject(this->loadProject(path));
    }

    void Manager::openProject(Project *project)
    {
//        DulyApp::currentCanvasInstance()->resetContent();
        m_project = project;
        m_project->declare();
        createTreeModel(m_project);
        m_currentPath = static_cast<models::TreeItem*>(m_projectModel->index(0,0).internalPointer());
        createNameSpaceModel(m_currentPath);
        createDeclarationModel(m_project);
//        views::DeclarationCanvas::CreateContext(m_project->main(), true);
    }

    void Manager::createDeclarationIfMissing(models::Common *c)
    {
        if (c->listIndex() + 1 > m_declRef->rowCount())
            for (auto i = 0; i < c->listIndex() + 1; i++)
            {
                if (i + 1 > m_declRef->rowCount())
                {
                    m_declRef->addDeclaration(new Declaration());
                }
            }
    }

    void Manager::setupContextModel(models::Context *context, int lvl)
    {
        if (!m_declRef || lvl == -1) return;
        lvl -= 1;
        const auto contexts = context->contexts();
        for (auto i = 0; i < contexts.size(); i++)
        {
            setupContextModel(contexts[i], lvl);
            createDeclarationIfMissing(contexts[i]);
            qvariant_cast<Declaration *>(m_declRef->data(m_declRef->index(contexts[i]->listIndex(), 0)))->addModel(contexts[i]);
        }

        const auto classes = context->classes();
        for (auto i = 0; i < classes.size(); i++)
        {
            setupClassModel(classes[i], lvl);
            createDeclarationIfMissing(classes[i]);
            qvariant_cast<Declaration *>(m_declRef->data(m_declRef->index(classes[i]->listIndex(), 0)))->addModel(classes[i]);
        }

        const auto functions = context->functions();
        for (auto i = 0; i < functions.size(); i++)
        {
            setupFunctionModel(functions[i]);
        }

        const auto variables = context->variables();
        for (auto i = 0; i < variables.size(); i++)
        {
            setupVariableModel(variables[i]);
        }
    }

    void Manager::setupClassModel(models::Class *cl, int lvl)
    {
        if (!m_declRef || lvl == -1) return;
        lvl -= 1;
        const auto classes = cl->classes();
        for (auto i = 0; i < classes.size(); i++)
        {
            setupClassModel(classes[i], lvl);
            createDeclarationIfMissing(classes[i]);
            qvariant_cast<Declaration *>(m_declRef->data(m_declRef->index(classes[i]->listIndex(), 0)))->addModel(classes[i]);
        }

        const auto functions = cl->functions();
        for (auto i = 0; i < functions.size(); i++)
        {
            setupFunctionModel(functions[i]);
        }

        const auto variables = cl->attributes();
        for (auto i = 0; i < variables.size(); i++)
        {
            setupVariableModel(variables[i]);
        }
    }

    void Manager::setupFunctionModel(models::Function *func)
    {
        if (!m_declRef) return;
        createDeclarationIfMissing(func);
        qvariant_cast<Declaration *>(m_declRef->data(m_declRef->index(func->listIndex(), 0)))->addModel(func);
    }

    void Manager::setupVariableModel(models::Variable *variable)
    {
        if (!m_declRef) return;
        createDeclarationIfMissing(variable);
        qvariant_cast<Declaration *>(m_declRef->data(m_declRef->index(variable->listIndex(), 0)))->addModel(variable);
    }

    void Manager::createDeclarationModel(Project *project)
    {
        clearDeclarationModel();
        setupContextModel(const_cast<duly_gui::models::Context *>(project->main()), 0);
    }

    void Manager::clearDeclarationModel()
    {
        m_declRef->clear();
    }

    void Manager::updateNamespace(const QModelIndex &index)

    {
        if (!m_declRef || index == QModelIndex()) return;
        m_currentPath = static_cast<models::TreeItem*>(index.internalPointer());
        createNameSpaceModel(m_currentPath);
        clearDeclarationModel();
        const auto model = m_currentPath->model();
        if (dynamic_cast<models::Context*>(model))
        {
            m_declRef->setIsContext(true);
            setupContextModel(static_cast<models::Context*>(static_cast<models::Common*>(model)), 0);
        }
        else if (dynamic_cast<models::Class*>(model))
        {
            m_declRef->setIsContext(false);
            setupClassModel(static_cast<models::Class*>(static_cast<models::Common*>(model)), 0);
        }
    }

    QModelIndex Manager::getIndexMatch(duly_gui::models::TreeItem *md)
    {
        if (!m_declRef || !md)
            return QModelIndex();
        auto res = m_projectModel->match(m_projectModel->index(0, 0),
                                         duly_gui::models::TreeModel::ItemRole,
                                         QVariant::fromValue(md),
                                         1, // look *
                                         Qt::MatchRecursive | Qt::MatchExactly);

        if (res.count())
            return res[0];
        return QModelIndex();
    }

    QModelIndex Manager::getIndexMatchFromModel(duly_gui::models::Common *md)
    {
        if (!m_declRef || !md)
            return QModelIndex();
        auto res = m_projectModel->match(m_projectModel->index(0, 0),
                                         duly_gui::models::TreeModel::ItemRole,
                                         QVariant::fromValue(md),
                                         1, // look *
                                         Qt::MatchRecursive | Qt::MatchContains);

        if (res.count())
            return res[0];
        return QModelIndex();
    }


    void Manager::selectTreeItem(const QModelIndex &index)
    {
        m_currentPath = static_cast<models::TreeItem*>(index.internalPointer());
        createNameSpaceModel(m_currentPath);
    }

    void Manager::setProjectModel(models::TreeModel *model)
    {
        if (model == nullptr || m_projectModel == model)
            return;
        m_projectModel = model;
        emit projectModelChanged(model);
    }

    void Manager::setNamespacebarModel(const QVariant &ref)
    {
        m_namespacebarmodel = ref;
        emit namespacebarModelChanged(ref);
    }

    void Manager::setDeclarationModel(DeclarationModel *m)
    {
        m_declRef = m;
        emit declarationModelChanged(m);
    }

    void Manager::createTreeModel(Project *project)
    {
        setProjectModel(new models::TreeModel(project));
    }

    void Manager::createNameSpaceModel(models::TreeItem *item)
    {
        QList<QObject *> temp;

        auto ns = new models::NameSpaceBarItem();
        ns->setPath(item->data(0).toString());
        ns->setAsChild(item->childCount());
        ns->setIdx(item->idxmodel());
        ns->setModel(item);
        temp.append(ns);
        while (item->parentItem()->parentItem())
        {
            item = item->parentItem();
            ns = new models::NameSpaceBarItem();
            ns->setPath(item->data(0).toString());
            ns->setAsChild(item->childCount());
            ns->setIdx(item->idxmodel());
            ns->setModel(item);
            temp.append(ns);
        }
        std::reverse(temp.begin(), temp.end());
        setNamespacebarModel(QVariant::fromValue(temp));
    }

    void Manager::addContext(int index, int listindex)
    {
        qvariant_cast<Declaration *>(m_declRef->data(m_declRef->index(listindex , 0)))->addModel(new models::Context("undefined", "Context", "", QVector2D(), index, listindex));
    }

    void Manager::addClass(int index, int listindex)
    {
        qvariant_cast<Declaration *>(m_declRef->data(m_declRef->index(listindex , 0)))->addModel(new models::Class("undefined", "Class", "", QVector2D(), index, listindex));
    }

    void Manager::addFunction(int index, int listindex)
    {
        qvariant_cast<Declaration *>(m_declRef->data(m_declRef->index(listindex , 0)))->addModel(new models::Function("undefined", "Function", "", QVector2D(), index, listindex));
    }

    void Manager::addVariable(int index, int listindex)
    {
        qvariant_cast<Declaration *>(m_declRef->data(m_declRef->index(listindex , 0)))->addModel(new models::Variable("undefined", "Variable", "", QVector2D(), "generic", false, index, listindex));
    }
}
