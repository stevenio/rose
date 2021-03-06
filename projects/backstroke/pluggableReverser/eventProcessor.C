#include "eventProcessor.h"
#include <boost/lexical_cast.hpp>
#include <utilities/utilities.h>

#include <VariableRenaming.h>

#include <boost/foreach.hpp>

#define foreach BOOST_FOREACH
#define reverse_foreach BOOST_REVERSE_FOREACH


using namespace std;
using namespace boost;
using namespace SageInterface;
using namespace SageBuilder;

/** A variable filter that considers every variable interesting. */
class DefaultVariableFilter : public IVariableFilter
{
public:

	virtual bool isVariableInteresting(const VariableRenaming::VarName&) const
	{
		return true;
	}
} defaultVariableFilter;

EventProcessor::EventProcessor(IVariableFilter* varFilter) : event_(NULL), var_renaming_(NULL), interproceduralSsa_(NULL)
{
	if (varFilter == NULL)
		variableFilter_ = &defaultVariableFilter;
	else
		variableFilter_ = varFilter;
}

void EventProcessor::addExpressionHandler(ExpressionReversalHandler* exp_handler)
{
	exp_handler->setEventHandler(this);
	exp_handlers_.push_back(exp_handler);
}

void EventProcessor::addStatementHandler(StatementReversalHandler* stmt_handler)
{
	stmt_handler->setEventHandler(this);
	stmt_handlers_.push_back(stmt_handler);
}

void EventProcessor::addVariableValueRestorer(VariableValueRestorer* restorer)
{
	restorer->setEventHandler(this);
	variableValueRestorers.push_back(restorer);
}

std::vector<EventReversalResult> EventProcessor::processEvent(SgFunctionDeclaration* event)
{
	event_ = event;
	return processEvent();
}

EvaluationResult EventProcessor::evaluateExpression(SgExpression* exp, const VariableVersionTable& var_table, bool is_value_used)
{
	foreach(ExpressionReversalHandler* exp_handler, exp_handlers_)
	{
		EvaluationResult res = exp_handler->evaluate(exp, var_table, is_value_used);

		if (res.isValid())
		{
			ROSE_ASSERT(res.getExpressionInput() == exp && res.getExpressionHandler() == exp_handler);
			return res;
		}
	}

	return EvaluationResult();
}

EvaluationResult EventProcessor::evaluateStatement(SgStatement* stmt, const VariableVersionTable& var_table)
{
	foreach(StatementReversalHandler* stmt_handler, stmt_handlers_)
	{
		EvaluationResult result = stmt_handler->evaluate(stmt, var_table);
		
		if (result.isValid())
		{
			return result;
		}
	}

	return EvaluationResult();
}

SgExpression* EventProcessor::restoreVariable(VariableRenaming::VarName variable, const VariableVersionTable& availableVariables,
		VariableRenaming::NumNodeRenameEntry definitions)
{
	vector<SgExpression*> results;

	//Check if we're already trying to restore this variable to this version. Prevents infinite recursion
	pair<VariableRenaming::VarName, VariableRenaming::NumNodeRenameEntry> variableAndVersion(variable, definitions);
	if (activeValueRestorations.count(variableAndVersion) > 0)
	{
		return NULL;
	}
	else
	{
		activeValueRestorations.insert(variableAndVersion);
	}


	//Check if the variable needs restoring at all. If it has the desired version, there is nothing to do
	if (availableVariables.matchesVersion(variable, definitions))
	{
		results.push_back(VariableRenaming::buildVariableReference(variable));
	}
	else
	{
		//Call the variable value restoration handlers

		foreach(VariableValueRestorer* variableRestorer, variableValueRestorers)
		{
			vector<SgExpression*> restorerOutput = variableRestorer->restoreVariable(variable, availableVariables, definitions);

			results.insert(results.end(), restorerOutput.begin(), restorerOutput.end());
		}
	}

	//Remove this variable from the active set
	activeValueRestorations.erase(variableAndVersion);

	//FIXME: Here, just pick the first restorer result. In the future we should use some model to help
	//us choose which result to use
	if (results.size() > 1)
	{
		for (size_t i = 1; i < results.size(); i++)
		{
			SageInterface::deepDelete(results[i]);
		}
		results.resize(1);
	}

	return results.empty() ? NULL : results.front();
}

bool EventProcessor::isStateVariable(SgExpression* exp)
{
	VariableRenaming::VarName var_name = VariableRenaming::getVarName(exp);
	if (var_name.empty())
		return false;
	return isStateVariable(var_name);
}

bool EventProcessor::isStateVariable(const VariableRenaming::VarName& var)
{
	// Currently we assume all variables except those defined inside the event function
	// are state varibles.
	return !SageInterface::isAncestor(
			BackstrokeUtility::getFunctionBody(event_),
			var[0]->get_declaration());

#if 0

	foreach(SgInitializedName* name, event_->get_args())
	{
		if (name == var[0])
			if (isSgPointerType(name->get_type()) || isReferenceType(name->get_type()))
				return true;
	}

	return false;
#endif
}

SgExpression* EventProcessor::pushVal(SgExpression* exp, SgType* returnType)
{
	SgScopeStatement* scope = isSgScopeStatement(event_->get_parent());
	ROSE_ASSERT(scope != NULL);
	SgExprListExp* parameters = SageBuilder::buildExprListExp(exp);
	return SageBuilder::buildFunctionCallExp("__push< " + get_type_name(returnType) + " >", returnType, parameters, scope);
}

SgExpression* EventProcessor::popVal(SgType* type)
{
	SgScopeStatement* scope = isSgScopeStatement(event_->get_parent());
	ROSE_ASSERT(scope != NULL);
	return SageBuilder::buildFunctionCallExp("__pop_back< " + get_type_name(type) + " >", type, NULL, scope);
}

SgExpression* EventProcessor::popVal_front(SgType* type)
{
cerr<<"DEBUG: EventProcessor::popVal_front :: P11.1"<<endl;
	string functionName = "pop_front< " + get_type_name(type) + " >";
cerr<<"DEBUG: EventProcessor::popVal_front :: P11.2"<<endl;
 cerr<<"DEBUG: buildFunctionCallExp("<<functionName<<type->unparseToString()<<")"<<endl;
 SgScopeStatement* scope = isSgScopeStatement(event_->get_parent());
 // TODO: investigate this fix
 ROSE_ASSERT(scope != NULL);
 SgExpression* exp=SageBuilder::buildFunctionCallExp(functionName, type, NULL,scope);
cerr<<"DEBUG: EventProcessor::popVal_front :: P11.3"<<endl;
	return exp;
}

SgExpression* EventProcessor::cloneValueExp(SgExpression* value, SgType* type)
{
	SgType* cleanedType = BackstrokeUtility::removePointerOrReferenceType(type);
	string typeName = get_type_name(cleanedType);
	string functionName = "__clone__< " + typeName + " >";

	SgType* returnType = SageBuilder::buildPointerType(cleanedType);

	SgExprListExp* params = SageBuilder::buildExprListExp(value);
	return SageBuilder::buildFunctionCallExp(functionName, returnType, params);
}

SgExpression* EventProcessor::assignPointerExp(SgExpression* lhs, SgExpression* rhs, SgType* lhsType, SgType* rhsType)
{
	string functionName = "__assign__< " + get_type_name(lhsType) + " , " + get_type_name(rhsType) + " >";
	SgType* returnType = SageBuilder::buildVoidType();
	SgExprListExp* params = SageBuilder::buildExprListExp(lhs, rhs);
	return SageBuilder::buildFunctionCallExp(functionName, returnType, params);
}

bool EventProcessor::checkForInitialVersions(const VariableVersionTable& var_table)
{
	typedef std::map<VariableRenaming::VarName, std::set<int> > TableType;

	foreach(const TableType::value_type& var, var_table.getTable())
	{
		if (isStateVariable(var.first))
		{
			if (var.second.size() != 1 || var.second.count(1) == 0)
			{
				return false;
			}
		}
	}
	return true;
}

std::vector<EventReversalResult> EventProcessor::processEvent()
{
	// Before processing, build a variable version table for the event function.
	VariableVersionTable var_table(event_, var_renaming_);

	SgBasicBlock* body = isSgFunctionDeclaration(event_->get_definingDeclaration())->get_definition()->get_body();
	ROSE_ASSERT(body);
	std::vector<EventReversalResult> outputs;

	SimpleCostModel cost_model;
	EvaluationResult reversalResult = evaluateStatement(body, var_table);
	ROSE_ASSERT(reversalResult.isValid());
	// Here we check the validity for each result above. We have to make sure
	// every state variable has the version 1.
	if (!checkForInitialVersions(reversalResult.getVarTable()))
		printf("WARNING: Not checking for correct initial versions!\n"); //continue;

	// Print all handlers used in this result.
	if (SgProject::get_verbose() > 0)
	{
		reversalResult.printHandlers();
		reversalResult.getCost().print();
		reversalResult.getVarTable().print();
	}
	StatementReversal stmt = reversalResult.generateReverseStatement();
	// Normalize the result.
	BackstrokeUtility::removeUselessBraces(stmt.forwardStatement);
	BackstrokeUtility::removeUselessBraces(stmt.reverseStatement);

	BackstrokeUtility::removeUselessParen(stmt.forwardStatement);
	BackstrokeUtility::removeUselessParen(stmt.reverseStatement);

#if 0
	// MS: no longer working in rose edg4x
   	SageInterface::fixVariableReferences(stmt.forwardStatement);
	SageInterface::fixVariableReferences(stmt.reverseStatement);
#else
	cerr<<"DEBUG: NOT fixing variable references (broken)."<<endl;
#endif
	string counterString = lexical_cast<string > (0);

	SgScopeStatement* eventScope = event_->get_scope();
	ROSE_ASSERT(eventScope);
	//Create the function declaration for the forward body
	SgName fwd_func_name = event_->get_name() + "_forward" + counterString;
	SgFunctionDeclaration* fwd_func_decl =
			SageBuilder::buildDefiningFunctionDeclaration(
			fwd_func_name, event_->get_orig_return_type(),
			isSgFunctionParameterList(copyStatement(event_->get_parameterList())),
			eventScope);
	SgFunctionDefinition* fwd_func_def = fwd_func_decl->get_definition();
	SageInterface::replaceStatement(fwd_func_def->get_body(), isSgBasicBlock(stmt.forwardStatement));

	//Create the function declaration for the reverse body
	SgName rvs_func_name = event_->get_name() + "_reverse" + counterString;
	SgFunctionDeclaration* rvs_func_decl =
			SageBuilder::buildDefiningFunctionDeclaration(
			rvs_func_name, event_->get_orig_return_type(),
			isSgFunctionParameterList(copyStatement(event_->get_parameterList())),
			eventScope);
	SgFunctionDefinition* rvs_func_def = rvs_func_decl->get_definition();
	SageInterface::replaceStatement(rvs_func_def->get_body(), isSgBasicBlock(stmt.reverseStatement));

	SgFunctionDeclaration* commitFunctionDecl = NULL;
	//MS: ROSE_ASSERT(stmt.commitStatement == NULL); //We'll worry about commit statements later
        if(!stmt.commitStatement == 0)
          cerr<<"WARNING: EventProcessor::processEvent: stmt.commitStatement != 0 "<<endl;
        

	// Add the cost information as comments to generated functions.
	string comment = "Cost: " + lexical_cast<string > (reversalResult.getCost().getCost());
	attachComment(fwd_func_decl, comment);
	attachComment(rvs_func_decl, comment);

	outputs.push_back(EventReversalResult(fwd_func_decl, rvs_func_decl, commitFunctionDecl));

	return outputs;
}

SgExpression* EventProcessor::restoreExpressionValue(SgExpression* expression, const VariableVersionTable& availableVariables)
{
	ROSE_ASSERT(expression != NULL);
	//Right now, if the expression has side effects we just assume we can't reevaluate it
	if (BackstrokeUtility::containsModifyingExpression(expression))
	{
		return NULL;
	}

	//Ok, so the expression has no side effects! We can just re-execute it
	//However, the variables used in the expression might have been changed between its location and the current node
	VariableRenaming::NumNodeRenameTable variablesInExpression = var_renaming_->getOriginalUsesAtNode(expression);

	//Go through all the variables used in the definition expression and check if their values have changed
	pair<VariableRenaming::VarName, VariableRenaming::NumNodeRenameEntry> nameDefinitionPair;
	SgExpression* expressionCopy = SageInterface::copyExpression(expression);

	foreach(nameDefinitionPair, variablesInExpression)
	{
		VariableRenaming::NumNodeRenameEntry originalVarVersion = nameDefinitionPair.second;

		if (!availableVariables.matchesVersion(nameDefinitionPair.first, originalVarVersion))
		{
			printf("Recursively restoring variable '%s' to its value at line %d.\n",
					VariableRenaming::keyToString(nameDefinitionPair.first).c_str(),
					expression->get_file_info()->get_line());

			//See if we can recursively restore the variable so we can re-execute the definition
			SgExpression* restoredOldValue = restoreVariable(nameDefinitionPair.first, availableVariables, originalVarVersion);
			if (restoredOldValue != NULL)
			{
				vector<SgExpression*> restoredVarReferences = BackstrokeUtility::findVarReferences(nameDefinitionPair.first, expressionCopy);

				foreach(SgExpression* restoredVarReference, restoredVarReferences)
				{
					printf("Replacing '%s' with '%s'\n", restoredVarReference->unparseToString().c_str(),
							restoredOldValue->unparseToString().c_str());

					//If the expression itself is a variable reference, eg (t = a),  we can't use SageInterface::replaceExpression
					//because the parent of the variable reference is null. Manually replace the expression with the restored value
					if (expressionCopy == restoredVarReference)
					{
						SageInterface::deepDelete(expressionCopy);
						expressionCopy = SageInterface::copyExpression(restoredOldValue);
						break;
					}

					SageInterface::replaceExpression(restoredVarReference, SageInterface::copyExpression(restoredOldValue));
				}

				SageInterface::deepDelete(restoredOldValue);
			}
			else
			{
				//There is a variable whose value we could not extract
				SageInterface::deepDelete(expressionCopy);
				return NULL;
			}
		}
	}

	//Ok, we restored all variables to the correct value. This should evaluate the same as the original expression
	return expressionCopy;
}

