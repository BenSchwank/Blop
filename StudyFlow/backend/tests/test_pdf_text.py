import os
import tempfile
import unittest

from fpdf import FPDF

from openrouter_genai import extract_pdf_text


class PdfTextTests(unittest.TestCase):
    def test_extracts_text_layer(self):
        pdf = FPDF()
        pdf.add_page()
        pdf.set_font("Helvetica", size=12)
        pdf.multi_cell(0, 8, "Die Ableitung von x^2 ist 2x.")
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "note.pdf")
            pdf.output(path)
            text = extract_pdf_text(path)
        self.assertIn("Ableitung", text)
        self.assertIn("2x", text)


if __name__ == "__main__":
    unittest.main()
