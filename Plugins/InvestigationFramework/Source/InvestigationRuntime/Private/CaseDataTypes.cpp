#include "CaseDataTypes.h"

const FCaseQuestion* FCaseContent::FindQuestion(FName QuestionId) const
{
	return Questions.FindByPredicate([QuestionId](const FCaseQuestion& Q) { return Q.Id == QuestionId; });
}

const FCaseQuestion* FCaseContent::FindQuestionForFact(FName FactId) const
{
	return Questions.FindByPredicate([FactId](const FCaseQuestion& Q)
	{
		return Q.Facts.ContainsByPredicate([FactId](const FCaseFact& F) { return F.Id == FactId; });
	});
}

const FCaseCharacter* FCaseContent::FindCharacter(FName CharacterId) const
{
	return Characters.FindByPredicate([CharacterId](const FCaseCharacter& C) { return C.Id == CharacterId; });
}

const FEvidenceItem* FCaseContent::FindEvidence(FName EvidenceId) const
{
	return Evidence.FindByPredicate([EvidenceId](const FEvidenceItem& E) { return E.Id == EvidenceId; });
}
