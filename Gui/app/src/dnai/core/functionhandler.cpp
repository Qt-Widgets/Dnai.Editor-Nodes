#include <functional>
#include <algorithm>

#include "dnai/core/functionhandler.h"

#include "dnai/commands/commandmanager.h"
#include "dnai/commands/corecommand.h"
#include "dnai/models/instruction.h"
#include "dnai/models/entity.h"

#include "dnai/editor.h"

using namespace std::placeholders;

namespace dnai
{
    namespace gcore
    {
        FunctionHandler::FunctionHandler(EntityManager &manager) :
            manager(manager),
            m_instruction(manager)
        {

        }

        void FunctionHandler::setup()
        {
            QObject::connect(&manager,  SIGNAL(entityAdded(::core::EntityID,models::Entity&)),
                             this,      SLOT(onEntityAdded(::core::EntityID,models::Entity&)));
            QObject::connect(&manager,  SIGNAL(entityRemoved(::core::EntityID,models::Entity&)),
                             this,      SLOT(onEntityRemoved(::core::EntityID,models::Entity&)));
            QObject::connect(this,              SIGNAL(instructionAdded(dnai::models::Entity*,dnai::models::gui::Instruction*)),
                             &m_instruction,    SLOT(onInstructionAdded(dnai::models::Entity*,dnai::models::gui::Instruction*)));
            QObject::connect(this,              SIGNAL(instructionRemoved(dnai::models::Entity*,dnai::models::gui::Instruction*)),
                             &m_instruction,    SLOT(onInstructionRemoved(dnai::models::Entity*,dnai::models::gui::Instruction*)));

            ::core::function::onEntryPointSet(std::bind(&FunctionHandler::onEntryPointSet, this, _1, _2));
            ::core::function::onSetEntryPointError(std::bind(&FunctionHandler::onSetEntryPointError, this, _1, _2, _3));
            ::core::function::onParameterSet(std::bind(&FunctionHandler::onParameterSet, this, _1, _2));
            ::core::function::onSetParameterError(std::bind(&FunctionHandler::onSetParameterError, this, _1, _2, _3));
            ::core::function::onReturnSet(std::bind(&FunctionHandler::onReturnSet, this, _1, _2));
            ::core::function::onSetReturnError(std::bind(&FunctionHandler::onSetReturnError, this, _1, _2, _3));
            ::core::function::onInstructionAdded(std::bind(&FunctionHandler::onInstructionAdded, this, _1, _2, _3, _4));
            ::core::function::onAddInstructionError(std::bind(&FunctionHandler::onAddInstructionError, this, _1, _2, _3, _4));
            ::core::function::onInstructionRemoved(std::bind(&FunctionHandler::onInstructionRemoved, this, _1, _2));
            ::core::function::onRemoveInstructionError(std::bind(&FunctionHandler::onRemoveInstructionError, this, _1, _2, _3));

            m_instruction.setup();
        }

        void FunctionHandler::onEntityAdded(::core::EntityID id, models::Entity &added)
        {
			Q_UNUSED(id)

			const auto containerId = added.coreModel()->data().containerId;
            if (!pendingParams.empty()
                && containerId == pendingParams.front().first
                && added.name() == pendingParams.front().second)
            {
                params.push(&added);
                setParameter(containerId, added.name(), false);
                pendingParams.pop();
            }
            else if (!pendingRet.empty()
                && containerId == pendingRet.front().first
                && added.name() == pendingRet.front().second)
            {
                returns.push(&added);
                setReturn(containerId, added.name(), false);
                pendingRet.pop();
            }
            else
            {
                models::gui::declarable::Function *func = getFunctionData(id);

                if (func != nullptr)
                {
                    for (models::Entity *child : added.childrenItem())
                    {
                        if (std::find_if(func->inputs().begin(), func->inputs().end(), [child](models::Entity *curr){ return curr->name() == child->name(); }) != func->inputs().end())
                        {
                            pendingParameter(id, child->name());
                        }
                        else if (std::find_if(func->outputs().begin(), func->outputs().end(), [child](models::Entity *curr){ return curr->name() == child->name(); }) != func->outputs().end())
                        {
                            pendingReturn(id, child->name());
                        }
                    }
                    func->setInputs(QList<models::Entity*>());
                    func->setOutputs(QList<models::Entity*>());

                    pendingFunctionInstructions.push_back(&added);
                }
            }

            refreshPendingFunctionInstructions();
        }

        void FunctionHandler::onEntityRemoved(EntityID id, models::Entity &removed)
        {
            Q_UNUSED(id)

			const auto containerId = removed.coreModel()->data().containerId;

            if (!pendingRmParam.empty()
                && pendingRmParam.front().first == containerId
                && pendingRmParam.front().second == removed.name())
            {
                models::Entity &func = manager.getEntity(containerId);

                func.guiModel<models::gui::declarable::Function>()->removeInput(removed.name());
                pendingRmParam.pop();
            }
            else if (!pendingRmRet.empty()
                     && pendingRmRet.front().first == containerId
                     && pendingRmRet.front().second == removed.name())
            {
                models::Entity &func = manager.getEntity(containerId);

                func.guiModel<models::gui::declarable::Function>()->removeOutput(removed.name());
                pendingRmRet.pop();
            }

            if (static_cast<core::ENTITY>(removed.entityType()) == core::ENTITY::FUNCTION)
            {
                pendingFunctionInstructions.remove(&removed);
            }
        }

        void FunctionHandler::pendingParameter(quint32 func, const QString &paramName)
        {
            pendingParams.push(std::make_pair(func, paramName));
        }

        void FunctionHandler::pendindRemoveParam(quint32 func, const QString &paramName)
        {
            pendingRmParam.push(std::make_pair(func, paramName));
        }

        void FunctionHandler::pendingReturn(quint32 func, const QString &returnName)
        {
            pendingRet.push(std::make_pair(func, returnName));
        }

        void FunctionHandler::pendingRmReturn(quint32 func, const QString &returnName)
        {
            pendingRmRet.push(std::make_pair(func, returnName));
        }

        void FunctionHandler::setEntryPoint(const quint32 function, quint32 instruction, bool save) const
        {
            qDebug() << "==Core== Function.SetEntryPoint(" << function << ", " << instruction << ") => save(" << save << ")";

            models::Entity &func = manager.getEntity(function);
            models::Function *data = func.guiModel<models::Function>();
            models::gui::Instruction *instr = data->getInstruction(instruction);

            commands::CommandManager::Instance()->exec(new commands::CoreCommand("Function.SetEntryPoint", save,
                [&func, instr](){
                    core::function::setEntryPoint(func.id(), instr->Uid());
                },
                nullptr //unset entry point not implemented
            ));
        }

        void FunctionHandler::setParameter(const quint32 func, QString const &paramName, bool save) const
        {
            models::Entity &function = manager.getEntity(func);

            if (getFunctionData(function.id()) != nullptr)
            {
                qDebug() << "==Core== Function.SetParameter(" << func << ", " << paramName << ") => save(" << save << ")";
                commands::CommandManager::Instance()->exec(
                    new commands::CoreCommand("Function.SetParameter", save,
                        std::bind(&::core::function::setParameter, function.id(), paramName),
                        nullptr /* not implemented yet */
                    )
                );
            }
        }

        void FunctionHandler::setReturn(const quint32 func, QString const &retName, bool save) const
        {
            models::Entity &function = manager.getEntity(func);

            if (getFunctionData(function.id()) != nullptr)
            {
                qDebug() << "==Core== Function.SetReturn(" << func << ", " << retName << ") => save(" << save << ")";

                commands::CommandManager::Instance()->exec(
                    new commands::CoreCommand("Function.SetReturn", save,
                        std::bind(&::core::function::setReturn, function.id(), retName),
                        nullptr /* not implemented yet */
                    )
                );
            }
        }

        void FunctionHandler::addInstruction(const quint32 func, quint32 instrType, const QList<quint32> &arguments, bool save) const
        {
            models::Entity &function = manager.getEntity(func);

            qDebug() << "==Core== Function.AddInstruction(" << func << ", " << static_cast<dnai::enums::QInstructionID::Instruction_ID>(instrType) << ", " << arguments << ") => save(" << save << ")";
            commands::CommandManager::Instance()->exec(new commands::CoreCommand("Function.AddInstruction", save,
                [&function, instrType, arguments](){
                    ::core::function::addInstruction(function.id(), static_cast<core::INSTRUCTION>(instrType), arguments.toStdList());
                },
                [this, &function, instrType](){
                    QString insId = function.guid().toString() + QString::number(instrType);

                    if (addedInstructions[insId].empty())
                    {
                        qWarning() << "==Core== Function.AddInstruction(undo): No such entity to remove";
                        throw std::runtime_error("AddedInstruction map is empty");
                    }

                    models::gui::Instruction *torm = addedInstructions[insId].back();

                    core::function::removeInstruction(function.id(), torm->Uid());
                }
            ));
        }

        void FunctionHandler::removeInstruction(quint32 func, quint32 instruction, bool save)
        {
            models::Entity &function = manager.getEntity(func);
            models::Function *data = function.guiModel<models::Function>();
            models::gui::Instruction *instr = data->getInstruction(instruction);
            std::list<quint32> construction = getConstructionList(instr);

            qDebug() << "==Core== Function.RemoveInstruction(" << func << ", " << static_cast<INSTRUCTION>(instruction) << ") => save(" << save << ")";

            commands::CommandManager::Instance()->exec(new commands::CoreCommand("Function.RemoveInstruction", save,
                [&function, data, instr](){

                    //remove data links
                    for (models::gui::IoLink *lnk : data->iolinks())
                    {
                        if (lnk->data().inputUuid == instr->guiUuid())
                            core::function::instruction::unlinkData(function.id(), instr->Uid(), lnk->data().inputName);
                        else if (lnk->data().outputUuid == instr->guiUuid())
                            core::function::instruction::unlinkData(function.id(), data->getInstruction(lnk->data().inputUuid)->Uid(), lnk->data().inputName);
                    }

                    //remove execution links
                    for (models::gui::FlowLink *lnk : data->flowlinks())
                    {
                        if (lnk->data().to == instr->guiUuid())
                        {
                            core::function::instruction::unlinkExecution(function.id(), data->getInstruction(lnk->data().from)->Uid(), lnk->data().outIndex);
                        }
                        else if (lnk->data().from == instr->guiUuid())
                        {
                            core::function::instruction::unlinkExecution(function.id(), instr->Uid(), lnk->data().outIndex);
                        }
                    }

                    core::function::removeInstruction(function.id(), instr->Uid());
                },
                [&function, instr, construction](){
                    core::function::addInstruction(function.id(), static_cast<INSTRUCTION>(instr->instruction_id()), construction);
                }
            ));
        }

        models::gui::declarable::Function *FunctionHandler::getFunctionData(::core::EntityID function, bool throws) const
        {
            if (manager.contains(function))
            {
                models::gui::declarable::Function *func = manager.getEntity(function).guiModel<models::gui::declarable::Function>();

                if (func != nullptr)
                    return func;
            }

            if (throws)
                throw std::runtime_error("Given entity is not a function");

            return nullptr;
        }

        std::list<quint32> FunctionHandler::getConstructionList(models::gui::Instruction *instr) const
        {
            std::list<quint32> construction;

            for (QString const &curr : instr->linked())
            {
                construction.push_back(manager.findByFullname(curr)->id());
            }
            return construction;
        }

        QList<QString> FunctionHandler::getLinkedEntities(std::list<quint32> const &construction) const
        {
            QList<QString> linked;

            for (quint32 curr : construction)
            {
                linked.append(manager.getEntity(curr).fullName());
            }
            return linked;
        }

        models::gui::Instruction *FunctionHandler::createInstruction(qint32 type, const std::list<quint32> &construction)
        {
            models::gui::Instruction *instr = new models::gui::Instruction();

            instr->setInstructionId(type);
            instr->setLinkedEntities(getLinkedEntities(construction));
            return instr;
        }

        /**
         * @brief FunctionHandler::refreshPendingFunctionInstructions
         *
         * This function tries to declared function instructions for thoses who needed entity that wasn't created yet
         *
         * Ex:
         *     - A function has a list of instructions
         *     - A getter is an instruction that need the id of a variable
         *     - If the variable is not created yet, we can't declare the getter
         *     - Then we need to wait until the variable is created
         *     - So we push the function in a list
         *     - Each time an entity is added, we try to redeclare the instructions
         *     - If the redeclaration went fine, we erase the function from the list
         *     - Either, we left the function in place
         */
        void FunctionHandler::refreshPendingFunctionInstructions()
        {
            //we visit all the functions from the list
            for (std::list<models::Entity *>::iterator it = pendingFunctionInstructions.begin(); it != pendingFunctionInstructions.end();)
            {
                //we get the function model
                models::Function *func = getFunctionData((*it)->id());

                //this queue is the instructions of the function
                std::queue<models::gui::Instruction *> instructions;
                //this queue is the list of entities to send
                std::queue<QList<quint32>> linked;

                //this is to check that we can declare all the instructions
                bool isfull = true;

                //we visit all the instructions of the function
                for (models::gui::Instruction *curr : func->instructions())
                {
                    //this is the list of entities to send
                    QList<quint32> construction;
                    //this is to check that all the entities exist
                    bool add = true;

                    //we visit all the entities to send for the instruction
                    for (QString const &cname : curr->linked())
                    {
                        models::Entity *associated = manager.findByFullname(cname);

                        //if the entity doesn't exists, we break the loop
                        if (associated == nullptr)
                        {
                            add = false;
                            break;
                        }
                        construction.append(associated->id());
                    }

                    //if the construction list is ok
                    if (add)
                    {
                        //we add it to the queue
                        instructions.push(curr);
                        linked.push(construction);
                    }
                    else
                    {
                        //in case of an error, we break the loop
                        isfull = false;
                        break;
                    }
                }

                //if we can declare all the instructions
                if (isfull)
                {
                    //declare everything
                    while (!instructions.empty())
                    {
                        pendingInstruction.push(instructions.front());
                        addInstruction((*it)->id(), instructions.front()->instruction_id(), linked.front(), false);
                        instructions.pop();
                        linked.pop();
                    }
                    //and remove the function from the list
                    it = pendingFunctionInstructions.erase(it);
                }
                else
                {
                    //or just increment to the next function
                    ++it;
                }
            }
        }

        QString FunctionHandler::getInstructionHash(QUuid funcguid, qint32 type) const
        {
            return funcguid.toString() + QString::number(type);
        }

        void FunctionHandler::rebuildInstructions(QList<models::gui::Instruction *> instructions)
        {
            QHash<models::Function *, QList<models::gui::IoLink *> >       iolinks;
            QHash<models::Function *, QList<models::gui::FlowLink *> >     flowlinks;

            for (models::gui::Instruction *instruction : instructions)
            {
                if (pendingRebuild.contains(instruction))
                {
                    continue;
                }
                else
                {
                    qDebug() << "==Core== Rebuild instruction: " << instruction << "(" << instruction->guiUuid() << " <=> " << instruction->Uid() << "): " << instruction->nodeMenuPath();

                    //get function data
                    models::Entity *func = instructionsFunction[instruction];
                    models::Function *data = func->guiModel<models::Function>();

                    //save links
                    for (models::gui::IoLink *curr : data->iolinks())
                    {
                        if (curr->data().inputUuid == instruction->guiUuid()
                                || curr->data().outputUuid == instruction->guiUuid())
                            iolinks[data].append(curr);
                    }

                    //save flows
                    for (models::gui::FlowLink *curr : data->flowlinks())
                    {
                        if (curr->data().from == instruction->guiUuid()
                                || curr->data().to == instruction->guiUuid())
                            flowlinks[data].append(curr);
                    }

                    //remove instruction
                    removeInstruction(func->id(), instruction->Uid(), false);

                    instruction->setInputs({});
                    instruction->setOutputs({});

                    //add instruction
                    addInstruction(func->id(), instruction->instruction_id(), QList<quint32>::fromStdList(getConstructionList(instruction)), false);

                    pendingRebuild.insert(instruction);
                }
            }

            for (QHash<models::Function *, QList<models::gui::IoLink *>>::iterator it = iolinks.begin(), end = iolinks.end(); it != end; ++it)
            {
                models::Function *data = it.key();

                for (models::gui::IoLink *curr : it.value())
                {
                    /*models::gui::Instruction *from = data->getInstruction(curr->data().outputUuid);
                    models::gui::Instruction *to = data->getInstruction(curr->data().inputUuid);

                    if (from && to && from->getOutputType(curr->data().outputName) == to->getInputType(curr->data().inputName))
                    {*/
                        models::gui::IoLink *toadd = new models::gui::IoLink();
                        toadd->setData(curr->data());
                        data->appendIoLink(toadd);
                    /*}
                    else
                    {
                        data->removeIoLink(curr);
                    }*/
                }
            }

            for (QHash<models::Function *, QList<models::gui::FlowLink *>>::iterator it = flowlinks.begin(), end = flowlinks.end(); it != end; ++it)
            {
                models::Function *data = it.key();

                for (models::gui::FlowLink *curr : it.value())
                {
                    models::gui::FlowLink *toadd = new models::gui::FlowLink();
                    toadd->setData(curr->data());
                    data->appendFlowLink(toadd);
                }
            }
        }

        void FunctionHandler::onEntryPointSet(quint32 function, quint32 instruction)
        {
            models::Entity &func = manager.getEntity(function);
            models::Function *data = func.guiModel<models::Function>();
            models::gui::Instruction *instr = data->getInstruction(instruction);

            qDebug() << "==Core== Function.EntryPointSet(" << function << ", " << instruction << ")";

            data->setEntryPoint(instr->guiUuid());
            commands::CoreCommand::Success();
            emit entryPointSet(&func, instr);
        }

        void FunctionHandler::onSetEntryPointError(quint32 function, quint32 instruction, const QString &msg) const
        {
            Q_UNUSED(function)
            Q_UNUSED(instruction)

            commands::CoreCommand::Error();
            Editor::instance().notifyError("Unable to set entry point: " + msg);
        }

        void FunctionHandler::onParameterSet(::core::EntityID func, const QString &paramName)
        {
            Q_UNUSED(func)
            Q_UNUSED(paramName)

            models::Entity &function = manager.getEntity(func);

            if (params.empty())
                return;

            models::Entity *param = params.front();

            models::gui::declarable::Function *gui = function.guiModel<models::gui::declarable::Function>();

            if (gui != nullptr && param->name() == paramName)
            {
                qDebug() << "==Core== Function.ParameterSet(" << func << ", " << paramName << ")";
                if (!gui->inputs().contains(param))
                    gui->addInput(param);
                if (!function.childrenItem().contains(param))
                    function.appendChild(param);
                commands::CoreCommand::Success();
                params.pop();
                emit parameterSet(&function, paramName);
            }
        }

        void FunctionHandler::onSetParameterError(::core::EntityID function, const QString &paramName, const QString &message) const
        {
            Q_UNUSED(function)
            Q_UNUSED(paramName)
            Q_UNUSED(message)

            commands::CoreCommand::Error();
            Editor::instance().notifyError("Unable to set variable " + paramName + " as parameter: " + message);
        }

        void FunctionHandler::onReturnSet(::core::EntityID function, const QString &returnName)
        {
            Q_UNUSED(function)
            Q_UNUSED(returnName)

            models::Entity &func = manager.getEntity(function);

            if (returns.empty())
                return;

            models::Entity *var = returns.front();

            models::gui::declarable::Function *gui = func.guiModel<models::gui::declarable::Function>();

            if (gui != nullptr && var->name() == returnName)
            {
                if (!gui->outputs().contains(var))
                    gui->addOutput(var);
                if (!func.childrenItem().contains(var))
                    func.appendChild(var);
                returns.pop();
                commands::CoreCommand::Success();
                qDebug() << "==Core== Function.ReturnSet(" << function << ", " << returnName << ")";
                emit returnSet(&func, returnName);
            }
        }

        void FunctionHandler::onSetReturnError(::core::EntityID function, const QString &returnName, const QString &message)
        {
            Q_UNUSED(function)
            Q_UNUSED(returnName)
            Q_UNUSED(message)

            commands::CoreCommand::Error();
            Editor::instance().notifyError("Unable to set return: " + message);
        }

        void FunctionHandler::onInstructionAdded(EntityID function, INSTRUCTION type, const std::list<EntityID> &arguments, InstructionID instruction)
        {
            models::Entity &func = manager.getEntity(function);
            models::Function *data = func.guiModel<models::Function>();
            models::gui::Instruction *instr;


            qDebug() << "==Core== Function.InstructionAdded(" << function << ", " << static_cast<dnai::enums::QInstructionID::Instruction_ID>(type) << ", " << QList<EntityID>::fromStdList(arguments) << ", " << instruction << ")";

            commands::CoreCommand::Success();

            QString instrId = getInstructionHash(func.guid(), static_cast<qint32>(type));

            if (!removedInstructions[instrId].empty())
            {
                instr = removedInstructions[instrId].back();
                removedInstructions[instrId].pop_back();
                data->addInstruction(instr);
                if (pendingRebuild.contains(instr))
                {
                    pendingRebuild.remove(instr);
                }
                else
                {
                    instr->setLinkedEntities(getLinkedEntities(arguments));
                }
            }
            else if (pendingInstruction.empty())
            {
                instr = createInstruction(type, arguments);
                data->addInstruction(instr);
            }
            else
            {
                instr = pendingInstruction.front();
                pendingInstruction.pop();
            }
            addedInstructions[instrId].push_back(instr);
            instr->setUid(instruction);
            instructionsFunction[instr] = &func;

            if (data->entryPoint() == instr)
                core::function::setEntryPoint(func.id(), instr->Uid());

            //need to call it now because it builds inputs and set position
            Editor::instance().finishInstructionBuilding(&func, instr);

            emit instructionAdded(&func, instr);
        }

        void FunctionHandler::onAddInstructionError(EntityID function, INSTRUCTION type, const std::list<EntityID> &arguments, const QString &messsage)
        {
            Q_UNUSED(function)
            Q_UNUSED(type)
            Q_UNUSED(arguments)

            commands::CoreCommand::Error();
            Editor::instance().notifyError("Unable to create instruction: " + messsage);
            emit addInstructionError(function, type, QList<quint32>::fromStdList(arguments), messsage);
        }

        void FunctionHandler::onInstructionRemoved(EntityID function, InstructionID instruction)
        {
            models::Entity &func = manager.getEntity(function);
            models::Function *data = func.guiModel<models::Function>();
            models::gui::Instruction *instr = data->getInstruction(instruction);
            QString instrId = getInstructionHash(func.guid(), instr->instruction_id());

            qDebug() << "==Core== Function.InstructionRemoved(" << function << ", " << instruction << ")";

            commands::CoreCommand::Success();

            if (data->entryPoint() == instr && !pendingRebuild.contains(instr))
                data->setEntryPoint(QUuid());

            data->removeInstruction(instr);

            removedInstructions[instrId].push_back(instr);
            addedInstructions[instrId].removeOne(instr);

            emit instructionRemoved(&func, instr);

            instructionsFunction.remove(instr);
        }

        void FunctionHandler::onRemoveInstructionError(EntityID funtion, InstructionID instruction, QString msg)
        {
            Q_UNUSED(funtion)
            Q_UNUSED(instruction)

            commands::CoreCommand::Error();
            Editor::instance().notifyError("Unable to remove instruction: " + msg);
        }

        InstructionHandler *FunctionHandler::instruction()
        {
            return &m_instruction;
        }
    }
}
