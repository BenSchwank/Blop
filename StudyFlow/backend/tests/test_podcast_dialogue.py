import unittest

from podcast_dialogue import parse_podcast_dialogue, podcast_voice_pair


class PodcastDialogueTests(unittest.TestCase):
    def test_labeled_turns_keep_speakers(self):
        script = """
ALEX: Was bedeutet das?
SAM: Kurz gesagt: eine Regel mit einem Beispiel.
ALEX: Und warum ist das wichtig?
SAM: Weil man damit den nächsten Schritt selbst lösen kann.
"""
        turns = parse_podcast_dialogue(script)
        self.assertEqual([speaker for speaker, _ in turns], ["ALEX", "SAM", "ALEX", "SAM"])
        self.assertIn("Regel", turns[1][1])

    def test_multiline_turn_stays_with_the_same_person(self):
        script = "ALEX: Erste Frage.\nKannst du das nochmal sagen?\nSAM: Ja, gerne."
        turns = parse_podcast_dialogue(script)
        self.assertEqual(turns[0][0], "ALEX")
        self.assertIn("nochmal", turns[0][1])
        self.assertEqual(turns[1][0], "SAM")

    def test_unlabeled_text_becomes_two_speakers(self):
        script = "Das ist der erste Gedanke. Hier kommt der zweite Satz.\n\nDanach erklärt jemand den Zusammenhang. Und nennt ein Beispiel."
        turns = parse_podcast_dialogue(script)
        speakers = {speaker for speaker, _ in turns}
        self.assertEqual(speakers, {"ALEX", "SAM"})

    def test_same_voice_choice_gets_a_second_person(self):
        self.assertEqual(podcast_voice_pair("aoede", "aoede"), ("aoede", "charon"))
        self.assertEqual(podcast_voice_pair("alloy", ""), ("aoede", "charon"))


if __name__ == "__main__":
    unittest.main()
