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

const FInterviewTopic* FCaseContent::FindTopic(FName TopicId) const
{
	return Topics.FindByPredicate([TopicId](const FInterviewTopic& T) { return T.Id == TopicId; });
}

const FCaseFact* FCaseContent::FindFact(FName FactId) const
{
	for (const FCaseQuestion& Question : Questions)
	{
		if (const FCaseFact* Fact = Question.Facts.FindByPredicate([FactId](const FCaseFact& F) { return F.Id == FactId; }))
		{
			return Fact;
		}
	}
	return nullptr;
}

const FInterviewResponse* FCaseCharacter::FindResponse(FName ResponseId) const
{
	return Responses.FindByPredicate([ResponseId](const FInterviewResponse& R) { return R.Id == ResponseId; });
}
