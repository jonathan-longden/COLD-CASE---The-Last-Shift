#include "CaseValidator.h"

#include "CaseDataTypes.h"
#include "CaseTruthTypes.h"

namespace CaseValidatorPrivate
{
	/** Adds Id to Seen, reporting empty or duplicate IDs. */
	static void CheckId(FName Id, const TCHAR* What, TSet<FName>& Seen, TArray<FString>& Problems)
	{
		if (Id.IsNone())
		{
			Problems.Add(FString::Printf(TEXT("A %s has no Id."), What));
			return;
		}
		bool bAlreadySeen = false;
		Seen.Add(Id, &bAlreadySeen);
		if (bAlreadySeen)
		{
			Problems.Add(FString::Printf(TEXT("Duplicate Id '%s' (%s)."), *Id.ToString(), What));
		}
	}

	static void CheckFactRefs(const TArray<FName>& FactIds, const TSet<FName>& KnownFacts, const FString& Where, TArray<FString>& Problems)
	{
		for (const FName FactId : FactIds)
		{
			if (!KnownFacts.Contains(FactId))
			{
				Problems.Add(FString::Printf(TEXT("%s refers to unknown Fact '%s'."), *Where, *FactId.ToString()));
			}
		}
	}
}

TArray<FString> FCaseValidator::Validate(const FCaseContent& Content, const FCaseTruth* Truth)
{
	using namespace CaseValidatorPrivate;

	TArray<FString> Problems;

	// One shared ID space for the whole case, so an ID can never mean two things.
	TSet<FName> AllIds;
	TSet<FName> FactIds;

	for (const FCaseQuestion& Question : Content.Questions)
	{
		CheckId(Question.Id, TEXT("Question"), AllIds, Problems);
		if (Question.Facts.Num() < 2)
		{
			Problems.Add(FString::Printf(TEXT("Question '%s' needs at least two possible Facts."), *Question.Id.ToString()));
		}
		for (const FCaseFact& Fact : Question.Facts)
		{
			CheckId(Fact.Id, TEXT("Fact"), AllIds, Problems);
			FactIds.Add(Fact.Id);
		}
	}

	for (const FName KeyQuestion : Content.KeyQuestions)
	{
		if (!Content.FindQuestion(KeyQuestion))
		{
			Problems.Add(FString::Printf(TEXT("KeyQuestions lists unknown Question '%s'."), *KeyQuestion.ToString()));
		}
	}

	for (const FInterviewTopic& Topic : Content.Topics)
	{
		CheckId(Topic.Id, TEXT("Topic"), AllIds, Problems);
	}

	for (const FCaseCharacter& Character : Content.Characters)
	{
		CheckId(Character.Id, TEXT("Character"), AllIds, Problems);
		if (Character.Responses.Num() > 0 && !Character.CanBeInterviewed())
		{
			Problems.Add(FString::Printf(TEXT("Character '%s' has Responses but no DefaultLine, so they can't be interviewed."), *Character.Id.ToString()));
		}
		for (const FInterviewResponse& Response : Character.Responses)
		{
			const FString Where = FString::Printf(TEXT("Response '%s'"), *Response.Id.ToString());
			CheckId(Response.Id, TEXT("Response"), AllIds, Problems);
			if (!Content.FindTopic(Response.Trigger) && !Content.FindEvidence(Response.Trigger))
			{
				Problems.Add(FString::Printf(TEXT("%s has Trigger '%s', which is not a Topic or an Evidence Id."), *Where, *Response.Trigger.ToString()));
			}
			if (Response.Line.IsEmpty())
			{
				Problems.Add(FString::Printf(TEXT("%s has no Line."), *Where));
			}
			CheckFactRefs(Response.AssertsFacts, FactIds, Where, Problems);
			for (const FName EvidenceId : Response.RequiresHeld)
			{
				if (!Content.FindEvidence(EvidenceId))
				{
					Problems.Add(FString::Printf(TEXT("%s RequiresHeld unknown Evidence '%s'."), *Where, *EvidenceId.ToString()));
				}
			}
			for (const FName EvidenceId : Response.RequiresNotHeld)
			{
				if (!Content.FindEvidence(EvidenceId))
				{
					Problems.Add(FString::Printf(TEXT("%s RequiresNotHeld unknown Evidence '%s'."), *Where, *EvidenceId.ToString()));
				}
			}
			if (Response.EndsCooperation && Character.RefusalLine.IsEmpty())
			{
				Problems.Add(FString::Printf(TEXT("%s ends cooperation but Character '%s' has no RefusalLine."), *Where, *Character.Id.ToString()));
			}
		}
	}

	// A flag that responses wait for but nothing ever sets is almost always a typo.
	TSet<FName> FlagsSet;
	for (const FCaseCharacter& Character : Content.Characters)
	{
		for (const FInterviewResponse& Response : Character.Responses)
		{
			FlagsSet.Append(Response.SetFlags);
		}
	}
	for (const FCaseCharacter& Character : Content.Characters)
	{
		for (const FInterviewResponse& Response : Character.Responses)
		{
			for (const FName Flag : Response.RequiresFlags)
			{
				if (!FlagsSet.Contains(Flag))
				{
					Problems.Add(FString::Printf(TEXT("Response '%s' requires flag '%s', which nothing sets."), *Response.Id.ToString(), *Flag.ToString()));
				}
			}
		}
	}

	for (const FEvidenceItem& Item : Content.Evidence)
	{
		CheckId(Item.Id, TEXT("Evidence"), AllIds, Problems);
		if (!Item.Author.IsNone() && !Content.FindCharacter(Item.Author))
		{
			Problems.Add(FString::Printf(TEXT("Evidence '%s' has unknown Author '%s'."), *Item.Id.ToString(), *Item.Author.ToString()));
		}
		if (Item.Findings.Num() == 0)
		{
			Problems.Add(FString::Printf(TEXT("Evidence '%s' has no Findings."), *Item.Id.ToString()));
		}
		for (const FEvidenceFinding& Finding : Item.Findings)
		{
			CheckId(Finding.Id, TEXT("Finding"), AllIds, Problems);
			CheckFactRefs(Finding.SupportsFacts, FactIds, FString::Printf(TEXT("Finding '%s'"), *Finding.Id.ToString()), Problems);
			for (const FEvidenceInterpretation& Interpretation : Finding.Interpretations)
			{
				CheckId(Interpretation.Id, TEXT("Interpretation"), AllIds, Problems);
				CheckFactRefs(Interpretation.SupportsFacts, FactIds, FString::Printf(TEXT("Interpretation '%s'"), *Interpretation.Id.ToString()), Problems);
			}
		}
	}

	if (Truth)
	{
		if (Truth->CaseId != Content.CaseId)
		{
			Problems.Add(FString::Printf(TEXT("Truth.json is for case '%s' but Case.json is '%s'."), *Truth->CaseId.ToString(), *Content.CaseId.ToString()));
		}
		CheckFactRefs(Truth->TrueFacts, FactIds, TEXT("Truth"), Problems);
		for (const FCaseQuestion& Question : Content.Questions)
		{
			int32 TrueCount = 0;
			for (const FCaseFact& Fact : Question.Facts)
			{
				TrueCount += Truth->TrueFacts.Contains(Fact.Id) ? 1 : 0;
			}
			if (TrueCount != 1)
			{
				Problems.Add(FString::Printf(TEXT("Question '%s' has %d true Facts in Truth.json; it needs exactly 1."), *Question.Id.ToString(), TrueCount));
			}
		}
	}

	return Problems;
}
