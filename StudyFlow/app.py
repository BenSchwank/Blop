import streamlit as st
import os
import google.generativeai as genai
from google import genai as google_genai
from google.genai import types
import faiss
import numpy as np
import json
import ast
import random
import time
import base64
import re
import logging
import threading
import shutil
import urllib.parse
import requests
import datetime
import uuid
from PyPDF2 import PdfReader

from dotenv import load_dotenv
from langchain_text_splitters import RecursiveCharacterTextSplitter
from langchain_community.vectorstores import FAISS
from langchain_google_genai import GoogleGenerativeAIEmbeddings
from fpdf import FPDF
import markdown
from data_manager import DataManager
from auth_manager import AuthManager


# Configure Logging
# Configure Logging
logging.basicConfig(level=logging.ERROR)

def generate_imagen_image(prompt):
    """Generates an image using Google GenAI (Imagen 3) and returns formatted HTML."""
    if "google_api_key" not in st.session_state and "GOOGLE_API_KEY" not in os.environ:
        return None
        
    try:
        # Pydantic/New SDK style
        client = google_genai.Client(api_key=os.environ.get("GOOGLE_API_KEY"))
        
        # Determine model - Imagen 3 is 'imagen-3.0-generate-001'
        # Or let's try to list or just use a known one.
        # User requested "Gemini Neo" / Imagen.
        
        response = client.models.generate_image(
            model='imagen-3.0-generate-001',
            prompt=prompt,
            config=types.GenerateImageConfig(
                number_of_images=1,
            )
        )
        
        if response.generated_images:
            img = response.generated_images[0]
            # It usually returns bytes or b64
            # SDK v0.2: image.image_bytes
            b64_data = base64.b64encode(img.image.image_bytes).decode('utf-8')
            return f'<img src="data:image/png;base64,{b64_data}" width="500" style="border-radius: 10px; margin: 10px 0;">'
            
    except Exception as e:
        print(f"Imagen Error: {e}")
        return None 
        # Will fallback to Pollinations if this returns None


def latex_to_html(text):
    """Converts LaTeX code to basic HTML for FPDF."""
    # 1. Preamble & Comments
    if "\\begin{document}" in text: text = text.split("\\begin{document}")[1]
    if "\\end{document}" in text: text = text.split("\\end{document}")[0]
    text = re.sub(r'%.*?$', '', text, flags=re.MULTILINE)
    
    # 2. Structure Removal
    text = re.sub(r'\\tableofcontents', '', text)
    text = re.sub(r'\\listoffigures', '', text)
    text = re.sub(r'\\maketitle', '', text)
    
    # 3. Images (TikZ & Prompt Markers)
    def get_poll_url(match):
        # Extract query or use default
        try: query = match.group(1) 
        except: query = "scientific diagram schematic"
        
        # 1. Try Google Imagen
        img_html = generate_imagen_image(query)
        if img_html:
            return f"<br>{img_html}<br>"
            
        # 2. Fallback to Pollinations
        encoded = urllib.parse.quote(query)
        return f'<br><img src="https://image.pollinations.ai/prompt/{encoded}?width=600&height=300&nologo=true&seed={int(datetime.datetime.now().timestamp())}" width="500"><br>'

    # Handle [IMAGE: ...] prompt markers
    text = re.sub(r'\[IMAGE: (.*?)\]', get_poll_url, text)
    # Handle TikZ diagrams
    text = re.sub(r'\\begin\{tikzpicture\}.*?\\end\{tikzpicture\}', '<br><br><b>[Abbildung (Generiert)]</b><br>' + get_poll_url(None), text, flags=re.DOTALL)
    text = re.sub(r'\\begin\{figure\}.*?\\end\{figure\}', '', text, flags=re.DOTALL)
    
    # 4. Headers (Styled)
    text = re.sub(r'\\section\*?\{(.*?)\}', r'<h1 style="color:#003366">\1</h1><br>', text)
    text = re.sub(r'\\subsection\*?\{(.*?)\}', r'<h2 style="color:#004C99">\1</h2><br>', text)
    text = re.sub(r'\\subsubsection\*?\{(.*?)\}', r'<h3>\1</h3><br>', text)
    
    # 5. Formatting (Bold, Italic)
    # Highlight important terms in Dark Red, Underline in Blue
    text = re.sub(r'\\textbf\{(.*?)\}', r'<b style="color:#990000">\1</b>', text)
    text = re.sub(r'\\textit\{(.*?)\}', r'<i>\1</i>', text)
    text = re.sub(r'\\underline\{(.*?)\}', r'<u style="color:#0033CC">\1</u>', text)
    text = re.sub(r'\\emph\{(.*?)\}', r'<i>\1</i>', text)
    
    # 6. Math Subscripts (U_b -> U_b)
    # Remove Math Mode Delimiters
    text = text.replace('$', '')
    # Convert Subscripts: X_y -> X<sub>y</sub>
    text = re.sub(r'(\w)_(\w)', r'\1<sub>\2</sub>', text)
    text = re.sub(r'(\w)_\{([^\}]+)\}', r'\1<sub>\2</sub>', text)
    # Convert Superscripts: X^y -> X<sup>y</sup>
    text = re.sub(r'(\w)\^(\w)', r'\1<sup>\2</sup>', text)
    
    # 7. Tables (Simplified)
    text = re.sub(r'\\begin\{tabular\}.*?', '', text)
    text = re.sub(r'\\begin\{longtable\}.*?', '', text)
    text = text.replace('\\end{tabular}', '')
    text = text.replace('\\end{longtable}', '')
    text = text.replace('\\hline', '')
    text = text.replace('&', ' | ')
    text = text.replace('\\\\', '<br>')
    
    # 8. Lists
    text = text.replace('\\begin{itemize}', '<ul>')
    text = text.replace('\\end{itemize}', '</ul>')
    text = text.replace('\\begin{enumerate}', '<ol>')
    text = text.replace('\\end{enumerate}', '</ol>')
    text = re.sub(r'\\item\s*', '<li>', text)
    text = text.replace('</li><li>', '</li><li>') # Simple fix implies manual closure not strictly needed in simple HTMLMixin sometimes but better safe:
    # FPDF HTMLMixin is tolerant.
    
    # 9. Cleanup
    text = re.sub(r'\\[a-zA-Z]+\{.*?\}', '', text) # Remove generic commands
    text = re.sub(r'\\[a-zA-Z]+', '', text) # Remove verbs
    text = text.replace('{', '').replace('}', '') # Remove braces
    
    # Newlines to BR
    text = re.sub(r'\n\s*\n', '<br><br>', text)
    text = text.replace('\n', ' ') # Inline newlines to space
    
    return text

def clean_json_output(text):
    """Sanitizes LLM output to extractvalid JSON/List."""
    # 1. Remove Markdown
    text = re.sub(r"```(json)?", "", text, flags=re.IGNORECASE).replace("```", "").strip()
    
    # 2. Extract outer list [...]
    match = re.search(r"\[.*\]", text, re.DOTALL)
    if match:
        text = match.group(0)
    
    # 3. Fix common syntax errors (Missing comma between objects)
    text = re.sub(r"\}\s*\{", "}, {", text)
    
    # 4. Remove comments (// ...)
    text = re.sub(r"//.*$", "", text, flags=re.MULTILINE)
    
    # 5. Fix unterminated strings (Nested quotes)
    # Strategy: Replace " inside values with '
    # This is a heuristic: If we see "word": "text "quote" text", we try to fix it.
    # A simple way is to use a lenient parser or regex to escape inner quotes, but that's hard.
    # Better: Direct instruction in prompt (we did that).
    # Fallback: Let's try to escape quotes that are not near : or , or { or }
    
    return text

def create_fallback_pdf(text, source_pdf_stream=None):
    """Generates a rich PDF locally using HTML conversion and extracting source images."""
    try:
        pdf = FPDF()
        pdf.add_page()
        pdf.set_auto_page_break(auto=True, margin=15)
        pdf.set_font("Helvetica", size=11)
        


        
        # HTML Title
        title_html = """
        <h1 align="center" style="color:#003366; font-size:24pt;">Zusammenfassung</h1>
        <h4 align="center" style="color:#666666;">(Rich Text & Originalgrafiken)</h4>
        <br><hr><br>
        """
        pdf.write_html(title_html)
        
        # Convert Body
        html_body = latex_to_html(text)
        
        # Unicode Safety (Basic Latin-1 transliteration)
        unicode_map = {
            'ä': 'ae', 'ö': 'oe', 'ü': 'ue', 'Ä': 'Ae', 'Ö': 'Oe', 'Ü': 'Ue', 'ß': 'ss',
            '“': '"', '”': '"', '–': '-', '…': '...'
        }
        for k, v in unicode_map.items():
            html_body = html_body.replace(k, v)
            
        pdf.write_html(html_body)
        
        # APPENDIX: Original Images from Source PDF
        if source_pdf_stream:
            try:
                import io
                if hasattr(source_pdf_stream, 'seek'): source_pdf_stream.seek(0)
                
                reader = PdfReader(source_pdf_stream)
                images_found = False
                
                count = 0
                for page in reader.pages:
                    if count > 20: break
                    if hasattr(page, 'images') and page.images:
                        for img in page.images:
                            if count > 20: break
                            
                            if not images_found:
                                pdf.add_page()
                                pdf.set_font("Helvetica", 'B', 16)
                                pdf.set_text_color(0, 51, 102)
                                pdf.cell(0, 10, "Anhang: Grafiken aus Original-PDF", ln=True)
                                pdf.ln(5)
                                images_found = True
                            
                            try:
                                pdf.image(io.BytesIO(img.data), w=160)
                                pdf.ln(5)
                                count += 1
                            except: pass
            except Exception as e_img:
                print(f"Extraction Error: {e_img}")
        
        return bytes(pdf.output())
    except Exception as e:
        print(f"Fallback PDF Error: {e}")
        return None

def create_study_plan_pdf(plan_data):
    """Generates a structured PDF from the study plan JSON."""
    try:
        pdf = FPDF()
        pdf.add_page()
        pdf.set_auto_page_break(auto=True, margin=15)
        pdf.set_font("Helvetica", size=11)
        
        # Title
        pdf.set_font("Helvetica", 'B', 18)
        pdf.set_text_color(0, 51, 102)
        pdf.cell(0, 10, "Mein Lernplan", ln=True, align='C')
        pdf.ln(5)
        
        pdf.set_font("Helvetica", 'I', 10)
        pdf.set_text_color(100, 100, 100)
        pdf.cell(0, 5, f"Erstellt am {datetime.datetime.now().strftime('%d.%m.%Y')}", ln=True, align='C')
        pdf.ln(10)
        
        for item in plan_data:
            # Wrapper for new vs old structure
            date = item.get('date', 'Datum')
            title = item.get('title', item.get('topic', 'Thema'))
            
            # Date & Topic Header
            pdf.set_fill_color(240, 248, 255) # Light Blue
            pdf.set_text_color(0, 0, 0)
            pdf.set_font("Helvetica", 'B', 12)
            pdf.cell(0, 8, f"{date}: {title}", ln=True, fill=True)
            
            # Construct Content
            html_content = ""
            
            # New Structure?
            if 'objectives' in item:
                # Objectives
                html_content += "<b>Ziele:</b><ul>"
                for obj in item['objectives']:
                    html_content += f"<li>{obj}</li>"
                html_content += "</ul><br>"
                
                # Topics
                for t in item.get('topics', []):
                    t_title = t.get('title', '')
                    t_desc = t.get('description', '')
                    t_time = t.get('time_minutes', 0)
                    t_rat = t.get('rationale', '')
                    
                    html_content += f"<b>{t_title}</b> ({t_time} min)<br>"
                    html_content += f"<i>{t_desc}</i><br>"
                    if t_rat:
                        html_content += f"<small style='color:#666'>Warum? {t_rat}</small><br>"
                    html_content += "<br>"
                    
                if item.get('review_focus'):
                     html_content += f"<b>Wiederholung:</b> {item['review_focus']}<br>"
            else:
                # Old Structure
                details = item.get('details', '')
                if details:
                    html_content = markdown.markdown(details)

            # Render HTML
            if html_content:
                # Unicode Safety
                unicode_map = {
                    'ä': 'ae', 'ö': 'oe', 'ü': 'ue', 'Ä': 'Ae', 'Ö': 'Oe', 'Ü': 'Ue', 'ß': 'ss',
                    '“': '"', '”': '"', '–': '-', '…': '...'
                }
                for k, v in unicode_map.items():
                    html_content = html_content.replace(k, v)
                
                pdf.set_font("Helvetica", size=11)
                pdf.write_html(html_content)
            
            pdf.ln(8)
            
        return bytes(pdf.output())
    except Exception as e:
        print(f"Plan PDF Error: {e}")
        return None
load_dotenv()

# Page Config
st.set_page_config(page_title="Blop Study", layout="wide")
st.title("Blop Study - Smart AI Learning Companion")

# --- Navigation State ---
if "current_page" not in st.session_state:
    st.session_state.current_page = "dashboard"
if "current_folder" not in st.session_state:
    st.session_state.current_folder = None
if "generated_summaries" not in st.session_state:
    st.session_state.generated_summaries = {} # {topic_title: summary_text}


def navigate_to(page, folder=None):
    st.session_state.current_page = page
    st.session_state.current_folder = folder
    st.rerun()

# --- Helper Functions ---

def create_markdown_pdf(md_text, source_pdf_stream=None):
    """Converts Markdown to PDF using FPDF."""
    try:
        # Convert MD -> HTML
        html_content = markdown.markdown(md_text)
        
        pdf = FPDF()
        pdf.add_page()
        pdf.set_auto_page_break(auto=True, margin=15)
        pdf.set_font("Helvetica", size=11)
        
        # Header
        pdf.write_html('<h1 align="center" style="color:#003366">Zusammenfassung</h1><br><hr><br>')
        
        # Robust Unicode Handling for FPDF (Helvetica)
        # 1. Manual Map for common German chars
        unicode_map = {
            'ä': 'ae', 'ö': 'oe', 'ü': 'ue', 'Ä': 'Ae', 'Ö': 'Oe', 'Ü': 'Ue', 'ß': 'ss',
            '“': '"', '”': '"', '–': '-', '…': '...'
        }
        for k, v in unicode_map.items():
            html_content = html_content.replace(k, v)
        
        # 2. Remove internal links (FPDF crash fix: "Named destination...")
        html_content = re.sub(r'<a href="#.*?">(.*?)</a>', r'\1', html_content)

        # 3. Bruteforce sanitize remainder (critical for FPDF stability)
        html_content = html_content.encode('latin-1', 'replace').decode('latin-1')
            
        pdf.write_html(html_content)
        
        # APPENDIX
        if source_pdf_stream:
            try:
                import io
                if hasattr(source_pdf_stream, 'seek'): source_pdf_stream.seek(0)
                reader = PdfReader(source_pdf_stream)
                images_found = False
                count = 0
                for page in reader.pages:
                    if count > 20: break
                    if hasattr(page, 'images') and page.images:
                        for img in page.images:
                            if count > 20: break
                            if not images_found:
                                pdf.add_page()
                                pdf.set_font("Helvetica", 'B', 16)
                                pdf.set_text_color(0, 51, 102)
                                pdf.cell(0, 10, "Anhang: Grafiken aus Original-PDF", ln=True)
                                pdf.ln(5)
                                images_found = True
                            try:
                                pdf.image(io.BytesIO(img.data), w=160)
                                pdf.ln(5)
                                count += 1
                            except: pass
            except: pass
            
        return bytes(pdf.output())
    except Exception as e:
        print(f"MD PDF Error: {e}")
        st.error(f"Fehler bei PDF-Erstellung (Fallback): {e}")
        return None

def get_generative_model_name():
    """Dynamically finds a working model name."""
    try:
        all_models = [m for m in genai.list_models() if 'generateContent' in m.supported_generation_methods]
        
        # Priority 1: High Quota Stable Model (1.5 Flash)
        for m in all_models:
            if "gemini-1.5-flash" in m.name and "001" in m.name: # e.g. gemini-1.5-flash-001
                return m.name
        for m in all_models:
            if "gemini-1.5-flash" in m.name:
                return m.name
                
        # Priority 2: Pro (Better quality, lower quota, but stable)
        for m in all_models:
            if "gemini-1.5-pro" in m.name: return m.name
        
        # Priority 3: Fallback to any flash (e.g. 2.5)
        for m in all_models:
            if "flash" in m.name: return m.name
            
        return all_models[0].name
    except:
        pass
    
    # Fallback default if list fails
    return "gemini-1.5-flash"

def get_embedding_model_name():
    """Dynamically finds a working embedding model name."""
    try:
        for m in genai.list_models():
            if 'embedContent' in m.supported_generation_methods:
                return m.name
    except:
        pass
    return "models/embedding-001"

# Removed cache to prevent stale data persistence
def get_pdf_text(_pdf_docs):
    pages_data = []
    for pdf in _pdf_docs:
        pdf_reader = PdfReader(pdf)
        for i, page in enumerate(pdf_reader.pages):
            t = page.extract_text()
            if t:
                # Store metadata
                pages_data.append({
                    "page": i + 1,
                    "text": t,
                    "source": pdf.name
                })
    return pages_data

def get_text_chunks(pages_data, chunk_size=1000, chunk_overlap=200):
    chunks = []
    for p in pages_data:
        text = p['text']
        page_num = p['page']
        source = p['source']
        
        start = 0
        while start < len(text):
            end = start + chunk_size
            chunk_text = text[start:end]
            # Chunk is now a Dict
            chunks.append({
                "text": chunk_text,
                "page": page_num,
                "source": source
            })
            start = end - chunk_overlap
    return chunks

def build_vector_store(text_chunks):
    # ... (Keep existing implementation)
    embeddings = []
    valid_chunks = []
    
    model = get_embedding_model_name()
    
    # Safe Mode: Process one chunk at a time to minimize complexity
    batch_size = 1 
    progress_bar = st.progress(0)
    total_chunks = len(text_chunks)
    
    for i in range(0, total_chunks, batch_size):
        batch = text_chunks[i:i + batch_size]
        # Extract plain text for embedding
        batch_text = [c['text'] for c in batch]
        
        retries = 0
        while retries < 5:
            try:
                # Batch embedding request
                result = genai.embed_content(
                    model=model,
                    content=batch_text,
                    task_type="retrieval_document",
                    title="Study Material"
                )
                
                # result['embedding'] is a list of embeddings when input is a list
                batch_embeddings = result['embedding']
                
                embeddings.extend(batch_embeddings)
                valid_chunks.extend(batch)
                
                # Success!
                # time.sleep(1) # Removed artificial delay for performance
                break
            except Exception as e:
                if "429" in str(e):
                    # Quote Exceeded? Wait significantly longer.
                    wait_time = 20 * (retries + 1) # 20s, 40s, 60s...
                    st.warning(f"Rate Limit (429) bei Index {i}. Warte {wait_time} Sekunden...")
                    time.sleep(wait_time)
                    retries += 1
                    if retries == 5:
                         st.error(f"Konnte nach {retries} Versuchen nicht fortfahren. Die API-Quota ist wahrscheinlich erschöpft. Bitte später versuchen.")
                else:
                    st.warning(f"Fehler bei Index {i}: {e}")
                    break
                    
        progress_bar.progress(min((i + batch_size) / total_chunks, 1.0))
        
    if not embeddings:
        return None, []
        
    embeddings_np = np.array(embeddings, dtype='float32')
    dimension = embeddings_np.shape[1]
    index = faiss.IndexFlatL2(dimension)
    index.add(embeddings_np)
    
    return index, valid_chunks

def answer_question(query, index, chunks):
    try:
        # Embed query
        embedding_model = get_embedding_model_name()
        result = genai.embed_content(
            model=embedding_model,
            content=query,
            task_type="retrieval_query"
        )
        q_emb = np.array([result['embedding']], dtype='float32')
        
        # Search
        D, I = index.search(q_emb, k=3)
        
        # Retrieve Context with Page Numbers
        context_chunks = [chunks[i] for i in I[0] if i < len(chunks)]
        context_text = ""
        for c in context_chunks:
            source_file = os.path.basename(c.get('source', 'Doc'))
            page_num = c.get('page', '?')
            context_text += f"\nSOURCE: {source_file} (Seite {page_num}):\n{c['text']}\n---"
        
        # Generate Answer
        model_name = get_generative_model_name()
        # Clean model name if necessary (remove 'models/' if sdk adds it automatically? No, sdk handles it)
        model = genai.GenerativeModel(model_name)
        
        prompt = f"""
        Beantworte die folgende Frage basierend auf dem bereitgestellten Kontext.
        Wenn die Antwort nicht im Kontext steht, sage das.
        
        WICHTIG: Gib die Seitenzahl an, wenn du Informationen aus dem Kontext verwendest (z.B. [Seite 5]).
        
        Frage: {query}
        
        Kontext:
        {context_text}
        """
        
        response = model.generate_content(prompt)
        return response.text, context_text
    except Exception as e:
        return f"Fehler bei der Antwortgenerierung: {str(e)}", ""

def generate_quiz(chunks):
    if not chunks:
        return None
        
    try:
        # Pick random chunks
        selected = random.sample(chunks, min(3, len(chunks)))
        context = ""
        for c in selected:
            context += f"\n[Seite {c.get('page','?')}]: {c['text']}\n"
        
        model_name = get_generative_model_name()
        model = genai.GenerativeModel(model_name)
        
        prompt = f"""
        Basierend auf folgendem Text, erstelle 3 Multiple-Choice-Fragen.
        Der Output MUSS valides JSON sein in diesem Format:
        [
            {{
                "question": "Frage hier (inkl. Seitenreferenz falls möglich)",
                "options": ["Option A", "Option B", "Option C", "Option D"],
                "answer": "Die richtige Option (exakter String)"
            }}
        ]
        
        Text:
        {context}
        """
        
        response = model.generate_content(prompt)
        
        # Parse JSON
        content = response.text
        if "```json" in content:
            content = content.split("```json")[1].split("```")[0]
        elif "```" in content:
            content = content.split("```")[1].split("```")[0]
        return json.loads(content)
    except Exception as e:
        st.error(f"Fehler bei Quiz-Generierung: {e}")
        return None

def generate_topic_summary(topic_title, context_chunks):
    """Generates a specific summary for a study topic."""
    try:
        # Simple RAG for the specific topic
        # We search the index if available, or just use the chunks provided
        # For simplicity/speed in this demo, we use the provided chunks (which might be the whole doc or a sample)
        
        # improved: filter chunks by simple keyword match to reduce noise if possible, else take all
        relevant_text = ""
        keywords = topic_title.split()
        count = 0
        for c in context_chunks:
            if any(k.lower() in c['text'].lower() for k in keywords):
                relevant_text += c['text'][:2000] + "\n"
                count += 1
            if count > 5: break
            
        if not relevant_text:
             # Fallback: take random chunks if no keyword match
             relevant_text = "\n".join([c['text'][:1000] for c in context_chunks[:3]])

        model_name = get_generative_model_name()
        model = genai.GenerativeModel(model_name)
        
        prompt = f"""
        **Aufgabe**: Erstelle eine ultrakurze, knackige Zusammenfassung (Flashcard-Style) für das Thema: '{topic_title}'.
        
        **Format**:
        - **Definition**: <mark>1 Satz Definition</mark>.
        - **Wichtig**: 3 Bulletpoints.
        - **Beispiel**: 1 kurzes Beispiel.
        - **Bild**: Wenn passend, füge `[IMAGE: kurze englische bildbeschreibung]` ein.
        
        **Kontext**:
        {relevant_text}
        """
        
        resp = model.generate_content(prompt)
        return render_visual_summary(resp.text)
    except Exception as e:
        return f"Konnte Zusammenfassung nicht erstellen: {e}"

def generate_full_summary(text_chunks, language="Deutsch", focus=""):
    """Generates a full summary of the provided text chunks."""
    try:
        all_text = "\n".join([c['text'] for c in text_chunks])[:50000]
        prompt = get_summary_prompt("Markdown", all_text, language=language, focus=focus)
        model = genai.GenerativeModel(get_generative_model_name())
        resp = model.generate_content(prompt)
        
        # Post-process to inject images
        final_text = render_visual_summary(resp.text)
        return final_text
    except Exception as e:
        return f"Fehler bei Generierung: {e}"



def generate_ics(plan_data, start_date):
    """Generates a basic iCalendar string from plan data."""
    cal_content = [
        "BEGIN:VCALENDAR",
        "VERSION:2.0",
        "PRODID:-//StudyFlow//Study Plan//EN",
        "CALSCALE:GREGORIAN",
        "METHOD:PUBLISH"
    ]
    
    # current_date = start_date # Not needed if we parse dates
    now_str = datetime.datetime.now().strftime("%Y%m%dT%H%M%SZ")
    
    for i, day_item in enumerate(plan_data):
        date_str_raw = day_item.get('date', '')
        event_date = None
        
        # Try to parse strict date format
        try:
            event_date = datetime.datetime.strptime(date_str_raw, "%d.%m.%Y").date()
        except:
            # Fallback: Use start_date + index
            event_date = start_date + datetime.timedelta(days=i)
            
        date_str = event_date.strftime("%Y%m%d")
        
        # Clean text
        summary = f"Lernen: {day_item.get('title', day_item.get('topic', 'Lerneinheit'))}"
        
        # New Structure vs Old
        if 'objectives' in day_item:
            desc_lines = ["Ziele:"] + [f"- {o}" for o in day_item['objectives']] + ["", "Inhalte:"]
            for t in day_item.get('topics', []):
                 desc_lines.append(f"* {t.get('title')} ({t.get('time_minutes')} min)")
                 desc_lines.append(f"  {t.get('description')}")
            desc = "\\n".join(desc_lines)
        else:
            desc = day_item.get('details', '').replace('\n', '\\n')
        
        # Unique ID
        uid = f"{date_str}-{i}@blop-study.app"
        
        cal_content.extend([
            "BEGIN:VEVENT",
            f"DTSTART;VALUE=DATE:{date_str}",
            f"DTSTAMP:{now_str}",
            f"UID:{uid}",
            f"SUMMARY:{summary}",
            f"DESCRIPTION:{desc}",
            "STATUS:CONFIRMED",
            "TRANSP:TRANSPARENT",
            "END:VEVENT"
        ])
        
    cal_content.append("END:VCALENDAR")
    return "\n".join(cal_content)

def get_summary_prompt(mode, text, language="Deutsch", focus=""):
    focus_instruction = ""
    if focus:
        focus_instruction = f"**FOKUS**: Der Nutzer möchte besonders folgendes beachtet haben: '{focus}'. Richte die Zusammenfassung stark danach aus!"
    else:
        focus_instruction = "**FOKUS**: Erstelle eine umfassende, neutrale Zusammenfassung, die alle Hauptthemen des Textes abdeckt. Identifiziere Schlüsselkonzepte und erkläre sie verständlich."

    # Anti-Hallucination Guard
    # Updated prompt with strict source rules
    focus_instruction += "\n\n**STRIKTE REGELN (QUELLE):**\n1. Nutze **AUSSCHLIESSLICH** den untenstehenden 'Input Text'.\n2. Ignoriere alles Wissen aus vorherigen Dokumenten oder Sitzungen.\n3. Nenne zu Beginn die **Quelle** (Dateiname) und das **Hauptthema**.\n4. Falls der Text Informationen enthält, die nicht zur aktuellen Datei gehören, ignoriere sie.\n5. Starte die Zusammenfassung mit: 'Diese Zusammenfassung basiert exakt auf der Datei [Dateiname]...'"

    if "LaTeX" in mode:
        return f"""
**Rolle & Ziel**
Du bist ein professioneller Typesetter (Setzer).
{focus_instruction}
**AUFGABE**: Konvertiere den folgenden **Markdown-Inhalt** (Input) exakt in das bereitgestellte **LaTeX-Template**.

**WICHTIG:**
1. **Inhalt**: Übernimm Text, Struktur, Überschriften und Bulletpoints **1:1**. **Schreibe KEINE neue Zusammenfassung**, sondern formatiere den existierenden Text!
2. **Tabellen**: Konvertiere Markdown-Tabellen in professionelle LaTeX-Tabellen (`booktabs`).
3. **Bilder**: Wenn du `[IMAGE: ...]` findest, ersetze es durch:
   `\\begin{{center}}\\textcolor{{accent}}{{\\textit{{[Hier Grafik einfügen: ...]}}}}\\end{{center}}` (oder TikZ, falls passend).

**Design-Elemente:**
1. **Highlight-Boxen**: Nutze `\\begin{{merkbox}}{{...}}...\\end{{merkbox}}` für Definitionen/Wichtiges.
2. **Farben**: `primary` (Blau) für Überschriften.
3. **Interaktivität**: Folgt aus `hyperref`.

**LaTeX-Template:**
\\documentclass[11pt, a4paper]{{article}}
\\usepackage[utf8]{{inputenc}}
\\usepackage[ngerman]{{babel}}
\\usepackage[T1]{{fontenc}}
\\usepackage{{helvet}}
\\renewcommand{{\\familydefault}}{{\\sfdefault}}
\\usepackage[left=2.5cm,right=2.5cm,top=2.5cm,bottom=2.5cm]{{geometry}}
\\usepackage{{amsmath}}
\\usepackage{{parskip}}
\\usepackage{{xcolor}}
\\usepackage{{tikz}}
\\usetikzlibrary{{arrows.meta, positioning, calc, shadows}}
\\usepackage{{tcolorbox}}
\\usepackage{{fancyhdr}}
\\usepackage{{titlesec}}
\\usepackage[hidelinks]{{hyperref}} % Interactive TOC & Links

% --- Design Definitions ---
\\definecolor{{primary}}{{HTML}}{{005BAA}} % Modern Blue
\\definecolor{{accent}}{{HTML}}{{E63946}}  % Red Accent

% Box Style
\\tcbuselibrary{{skins}}
\\newtcolorbox{{merkbox}}[1]{{
  colback=primary!5!white,
  colframe=primary,
  title={{#1}},
  fonttitle=\\bfseries\\sffamily,
  boxrule=0.5mm,
  arc=2mm,
  drop shadow
}}

% Header/Footer
\\pagestyle{{fancy}}
\\fancyhf{{}}
\\lhead{{\\textcolor{{primary}}{{\\textbf{{Blop Study}}}}}}
\\rhead{{\\today}}
\\cfoot{{\\thepage}}
\\renewcommand{{\\headrulewidth}}{{0.4pt}}
\\renewcommand{{\\headrule}}{{\\hbox to\\headwidth{{\\color{{primary}}\\leaders\\hrule height \\headrulewidth\\hfill}}}}

% Section Styling
\\titleformat{{\\section}}
  {{\\color{{primary}}\\Large\\bfseries\\uppercase}}
  {{\\thesection}}{{1em}}{{}}

\\begin{{document}}

% --- Modern Title ---
\\thispagestyle{{empty}}
\\begin{{tikzpicture}}[remember picture, overlay]
    \\node[fill=primary, minimum width=\\paperwidth, minimum height=3cm, anchor=north] at (current page.north) {{}};
    \\node[anchor=north, text=white, yshift=-1cm] at (current page.north) {{\\fontsize{{30}}{{36}}\\selectfont \\textbf{{Zusammenfassung}}}};
\\end{{tikzpicture}}
\\vspace{{3cm}}

\\begin{{center}}
    {{\\Huge \\textbf{{[TITEL]}}}} \\\\[0.5em]
    {{\\Large \\textcolor{{gray}}{{Skript & Dokumentation}}}} \\\\[2em]
    \\today
\\end{{center}}
\\vspace{{1cm}}

\\tableofcontents
\\newpage

% --- Content ---
% KI: Hier den Markdown-Input in LaTeX konvertieren.

\\end{{document}}

**Input Markdown Content:**
{text[:50000]}

**Constraints:**
- Return ONLY valid LaTeX code. No Markdown blocks.
"""
    else:
        return f"""
**Rolle**: Du bist ein Lern-Assistent für Studenten ("Study Buddy").
**Sprache**: {language} (Antworte zwingend in dieser Sprache!).

**Aufgabe**:
Erstelle eine extrem strukturierte Zusammenfassung für eine Lern-App.
Der Nutzer will effizient lernen.

{focus_instruction}

**Output Struktur (WICHTIG)**:
1. **Thematische Blöcke**: Teile den Inhalt in logische Abschnitte.
    2. **IDs**: Nutze KEINE manuellen HTML-Anker. Nutze stattdessen Standard-Markdown-Überschriften.
    3. **Formatierung**:
       - Nutze `<mark>markierter Text</mark>` für Definitionen und Schlüsselbegriffe.
       - Nutze **Fett** für Wichtiges.
       - Nutze Emojis.
    4. **Bilder**: Füge nach jedem Hauptabschnitt ein passendes Bild ein mit: `[IMAGE: visual description in english]`.
       Bsp: `[IMAGE: schematic diagram of cell division]`

    **Inhaltliches Format**:
    # Zusammenfassung

    ## Inhaltsverzeichnis
    - [1. Titel Thema](#1-titel-thema) (Achte auf Kleinschreibung und Bindestriche!)
    - [2. Titel Thema](#2-titel-thema)

    ## 1. Titel Thema
    ... Inhalt ...
    Definition: <mark>Begriff</mark> ist ...

    ## 2. Titel Thema
    ...

**Input Text**:
{text}
"""

import urllib.parse

@st.cache_data(show_spinner=False)
def render_visual_summary(text):
    """Replaces [IMAGE: query] with Pollinations.ai image URLs."""
    if not text: return ""
    def replacer(match):
        query = match.group(1).strip()
        # Clean query: Remove special chars that might break URL
        # Allowing alphanumeric, commas, spaces
        clean_query = re.sub(r'[^a-zA-Z0-9,\s]', '', query)
        encoded = urllib.parse.quote(clean_query)
        # Random seed to prevent caching issues (on API side), but we cache the result function
        seed = int(datetime.datetime.now().timestamp()) + random.randint(0, 1000)
        # REMOVED nologo=true to avoid rate limit
        return f"![Bild: {clean_query}](https://image.pollinations.ai/prompt/{encoded}?width=1024&height=512&seed={seed})"
    
    return re.sub(r'\[IMAGE: (.*?)\]', replacer, text)

def get_overleaf_url(latex_code):
    """Generates a direct Overleaf opening link."""
    try:
        # Encode as Base64 Data URI
        b64 = base64.b64encode(latex_code.encode('utf-8')).decode('utf-8')
        return f"https://www.overleaf.com/docs?snip_uri=data:application/x-tex;base64,{b64}"
    except:
        return None

def compile_latex_to_pdf(latex_code, fallback_markdown=None):
    """Compiles LaTeX to PDF using multiple Cloud APIs (Fallback Strategy)."""
    headers = {
        'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36'
    }
    
    # Strategy 1: LatexOnline (HTTP)
    try:
        url = "http://latexonline.cc/compile"
        files = {'file': ('main.tex', latex_code)}
        response = requests.post(url, files=files, headers=headers, timeout=30)
        if response.status_code == 200: return response.content
    except: pass 
        
    # Strategy 2: RTeX (HTTP)
    try:
        url_api = "http://rtex.probablyaweb.site/api/v2"
        response = requests.post(url_api, data={'code': latex_code, 'format': 'pdf'}, headers=headers, timeout=30)
        if response.status_code == 200:
            data = response.json()
            if data.get('status') == 'success':
                pdf_url = f"{url_api}/{data['filename']}"
                pdf_resp = requests.get(pdf_url, headers=headers, timeout=60)
                if pdf_resp.status_code == 200: return pdf_resp.content
    except: pass
        
    st.warning("⚠️ Cloud-Server nicht erreichbar (Firewall). Generiere lokale Notfall-PDF.")
    
    # Try to get source PDF for images (Appendix)
    source_pdf = None
    if "uploaded_file" in st.session_state and st.session_state.uploaded_file:
        try:
            source_pdf = st.session_state.uploaded_file
        except: pass
        
    if fallback_markdown:
        return create_markdown_pdf(fallback_markdown, source_pdf)
    
    return None

def render_page_buttons(text):
    """Finds 'Seite X' in text and renders buttons to jump there."""
    if not text: return
    
    # Regex to find "Seite 5", "Seite 12", "[Seite 5]"
    matches = re.findall(r"Seite\s+(\d+)", text)
    unique_pages = sorted(list(set(matches)), key=lambda x: int(x))
    
    if unique_pages:
        st.write("🔎 **Fundstellen:**")
        # Use more columns for smaller buttons
        cols = st.columns(min(len(unique_pages), 10)) 
        for i, p in enumerate(unique_pages):
            if i < 20: # Limit total buttons
                col_idx = i % 10
                # Minimal button label "[5]"
                if cols[col_idx].button(f"[{p}]", key=f"jump_{p}_{hash(text)}", help=f"Gehe zu Seite {p}"):
                    st.session_state.current_pdf_page = int(p)
                    st.toast(f"Seite {p}")

def display_pdf(uploaded_file):
    """Generates HTML iframe for PDF display."""
    try:
        if hasattr(uploaded_file, 'seek'):
            uploaded_file.seek(0)
        bytes_data = uploaded_file.getvalue()
        base64_pdf = base64.b64encode(bytes_data).decode('utf-8')
        
        # Check for requested page
        page_num = st.session_state.get('current_pdf_page', 1)
        offset = st.session_state.get('page_offset', 0)
        
        real_page = max(1, page_num + offset)
        
        # Append timestamp to force reload (anti-cache)
        t = time.time()
        
        # Append #page=X
        pdf_display = f'<iframe src="data:application/pdf;base64,{base64_pdf}#page={real_page}&zoom=100&t={t}" width="100%" height="800px" type="application/pdf"></iframe>'
        return pdf_display
    except Exception as e:
        return f"Fehler bei PDF-Anzeige: {e}"

def display_pdf_bytes(pdf_bytes):
    """Generates HTML iframe from specific PDF bytes."""
    try:
        base64_pdf = base64.b64encode(pdf_bytes).decode('utf-8')
        t = time.time()
        pdf_display = f'<iframe src="data:application/pdf;base64,{base64_pdf}#view=FitH&t={t}" width="100%" height="800px" type="application/pdf"></iframe>'
        return pdf_display
    except Exception as e:
        return f"Fehler: {e}"

# ... (Sidebar code unchanged) ...

# ... (Tab 1 Code Update) ...
# I will supply the full replacement here to ensure model usage is consistent
# ... inside the button logic ...
                # model = genai.GenerativeModel(get_generative_model_name()) 


# --- UI COMPONENTS ---

@st.dialog("⚠️ Konto löschen")
def show_delete_account_dialog(username):
    st.error("Bist du sicher? Alle deine Daten (Projekte, PDFs, Pläne) werden unwiderruflich gelöscht.")
    
    st.warning("Diese Aktion kann nicht rückgängig gemacht werden.")
    
    col1, col2 = st.columns(2)
    with col1:
        if st.button("Abbrechen", use_container_width=True):
            st.rerun()
    with col2:
        if st.button("Endgültig löschen", type="primary", use_container_width=True):
            if AuthManager.delete_user(username):
                st.success("Konto gelöscht.")
                time.sleep(1)
                AuthManager.logout()
            else:
                st.error("Fehler beim Löschen.")

def render_sidebar():
    with st.sidebar:
        st.title("⚡ Blop AI")
        
        # DB Status
        db = DataManager._init_firestore()
        if db:
            st.caption("🟢 Speicher: Cloud (Firestore)")
        else:
            err = st.session_state.get("db_error", "Unbekannter Fehler")
            st.caption(f"🔴 Speicher: Lokal (Fehler: {err})")
        
        # Navigation
        if st.button("🏠 Dashboard", use_container_width=True, type="secondary"):
            navigate_to("dashboard")
        
        st.divider()
        
        # Settings
        with st.expander("⚙️ Einstellungen"):
            api_key_input = st.text_input("Gemini API Key", type="password", key="api_key_input")
            if api_key_input:
                os.environ["GOOGLE_API_KEY"] = api_key_input
                genai.configure(api_key=api_key_input)
                st.success("Gespeichert!")
            
            models = ["Automatisch", "gemini-1.5-flash", "gemini-1.5-pro"]
            sel = st.selectbox("Modell", models, key="model_selector")
            st.session_state.model_option = sel
            
        st.divider()
        
        # Profile
        username = st.session_state.get("username", "Gast")
        st.caption(f"Angemeldet als: {username}")
        if st.button("Logout", type="primary", use_container_width=True):
            AuthManager.logout()
            
        st.divider()
        if st.button("Konto löschen", type="secondary", use_container_width=True):
            show_delete_account_dialog(username)

def _run_analysis(username, folder_id):
    """Helper to analyze all PDFs in the folder."""
    pdfs = DataManager.list_pdfs(username, folder_id)
    if not pdfs: return
    
    full_paths = [DataManager.get_pdf_path(p, username, folder_id) for p in pdfs]
    
    with st.spinner(f"{len(pdfs)} Dokumente werden analysiert..."):
        try:
            # Create file-like objects with 'name' attribute for get_pdf_text compatibility
            class FileObj:
                def __init__(self, path):
                    self.path = path
                    self.name = os.path.basename(path)
                    self._f = open(path, "rb")
                def read(self, n=-1): return self._f.read(n)
                def seek(self, offset, whence=0): return self._f.seek(offset, whence)
                def tell(self): return self._f.tell()
                def close(self): self._f.close()
            
            file_objs = [FileObj(p) for p in full_paths]
            
            # CLEAR OLD STATE explicitly
            if "text_chunks" in st.session_state: del st.session_state.text_chunks
            if "vector_index" in st.session_state: del st.session_state.vector_index
            
            # Re-use existing analysis logic
            raw_text = get_pdf_text(file_objs) 
            chunks = get_text_chunks(raw_text)
            index, valid_chunks = build_vector_store(chunks)
            
            st.session_state.vector_index = index
            st.session_state.text_chunks = valid_chunks
            # Cleanup
            for f in file_objs: f.close()
            st.rerun()
            
        except Exception as e:
            st.error(f"Fehler bei Analyse: {e}")

def render_file_manager(username, folder_id):
    st.subheader("📂 Datei-Manager")
    
    # 1. Upload Area (Form)
    with st.form(f"upload_form_{folder_id}", clear_on_submit=True):
        c_up1, c_up2 = st.columns([4, 1])
        with c_up1:
            uploaded_files = st.file_uploader("PDFs hochladen", type=["pdf"], accept_multiple_files=True, label_visibility="collapsed")
        with c_up2:
            submitted = st.form_submit_button("Hochladen", use_container_width=True)
        
        if submitted and uploaded_files:
            # We just save the PDFs here. Rerun triggers the list update.
            # Latency note: Might need a brief sleep or manual "pending" state?
            for f in uploaded_files:
                DataManager.save_pdf(f, username, folder_id)
            st.success("Dateien hochgeladen!")
            time.sleep(1) 
            st.rerun()

    st.divider()
    
    # 2. List ALL Files (PDF, Plan, Summary)
    all_files = DataManager.list_files(username, folder_id)
    
    if not all_files:
        st.info("Dieser Ordner ist noch leer.")
    else:
        # Display as Grid of "Cards" (using columns)
        cols = st.columns(4) # 4 items per row
        for i, file_obj in enumerate(all_files):
            col = cols[i % 4]
            with col:
                with st.container(border=True):
                    # ICON based on type
                    ftype = file_obj.get("type", "pdf")
                    icon = "📄"
                    if ftype == "plan": icon = "📅"
                    if ftype == "summary": icon = "📝"
                    
                    st.markdown(f"### {icon}")
                    st.caption(file_obj.get("name", "Unbenannt")[:20]) # Truncate name
                    
                    # Action: Open
                    if st.button("Öffnen", key=f"open_{file_obj['id']}", use_container_width=True):
                        # Set selection in session state to open dialog/overlay
                        st.session_state.selected_file = file_obj
                        st.session_state.show_file_overlay = True
                        st.rerun()

    # 3. Handle Overlay/Dialog for Selected File
    if st.session_state.get("show_file_overlay") and st.session_state.get("selected_file"):
        sel_file = st.session_state.selected_file
        ftype = sel_file.get("type", "pdf")
        
        if ftype == "pdf":
            show_pdf_overlay_dialog(0, username, folder_id, filename=sel_file["name"])
        elif ftype == "plan":
            # We need to render the Coach Dialog but for this specific plan content
            # For now, let's re-use the coach logic IF it's the main plan
            # Or render a read-only view? 
            # Implementation Plan said "Open Plan Dialog". 
            # Let's show the interactive coach for the plan data.
            st.toast(f"Öffne Lernplan: {sel_file['name']}")
            # We might need to load this plan into the main session state or show a dedicated dialog
            # Since our coach logic relies on st.session_state.plan_data, we should load it there?
            # Or separate "Viewing" from "Coaching"?
            # Let's assume user wants to EDIT/VIEW this plan.
            st.session_state.plan_data = sel_file.get("content", [])
            st.session_state.show_file_overlay = False # Close overlay trigger, we successfully loaded it
            st.rerun()
            
        elif ftype == "summary":
            show_summary_dialog(sel_file["name"], sel_file.get("content", ""))
            st.session_state.show_file_overlay = False # Close after rendering? Dialogs persist?
            # st.dialog behaves like a modal. logic loop needs check.
            pass

    # Clean up overlay state if dialog closed? (Streamlit dialogs handle this themselves mostly)


# --- GOOGLE AUTH HELPERS ---
def get_google_auth_url():
    try:
        client_id = st.secrets["google"]["client_id"]
        redirect_uri = st.secrets["google"]["redirect_uri"]
        return f"https://accounts.google.com/o/oauth2/v2/auth?response_type=code&client_id={client_id}&redirect_uri={redirect_uri}&scope=openid%20email%20profile"
    except: return None

def get_google_user(code):
    try:
        # Exchange Code
        token_url = "https://oauth2.googleapis.com/token"
        data = {
            "code": code,
            "client_id": st.secrets["google"]["client_id"],
            "client_secret": st.secrets["google"]["client_secret"],
            "redirect_uri": st.secrets["google"]["redirect_uri"],
            "grant_type": "authorization_code"
        }
        r = requests.post(token_url, data=data)
        token = r.json().get("access_token")
        
        # Get User Info
        user_info = requests.get("https://www.googleapis.com/oauth2/v1/userinfo", headers={"Authorization": f"Bearer {token}"}).json()
        return user_info
    except Exception as e:
        st.error(f"Auth Error: {e}")
        return None

# --- DASHBOARD & WORKSPACE LOGIC ---
def render_login_screen():
    st.title("🔐 Blop Study Anmeldung")
    
    # Check for Google Callback
    if "code" in st.query_params:
        code = st.query_params["code"]
        with st.spinner("Verbinde mit Google..."):
            user_info = get_google_user(code)
            if user_info:
                email = user_info.get("email")
                name = user_info.get("name", email)
                
                # Auto-Register Google User in AuthManager if new
                # We use email as username so data is consistent
                # Generate random secure password since they use Google to login
                AuthManager.register(email, str(uuid.uuid4()))
                
                st.session_state.authenticated = True
                st.session_state.username = email
                
                st.success(f"Willkommen, {name}!")
                st.query_params.clear()
                st.rerun()

    if "auth_mode" not in st.session_state:
        st.session_state.auth_mode = "login"

    # Toggle Functions
    def show_register(): st.session_state.auth_mode = "register"
    def show_login(): st.session_state.auth_mode = "login"

    container = st.container()
    
    with container:
        # --- LOGIN VIEW ---
        if st.session_state.auth_mode == "login":
            st.subheader("Einloggen")
            
            # Google Button
            auth_url = get_google_auth_url()
            if auth_url:
                st.link_button("Mit Google anmelden", auth_url, type="primary", use_container_width=True)
                st.markdown("""<div style="text-align: center; margin: 10px 0; color: #666;">- oder -</div>""", unsafe_allow_html=True)

            l_user = st.text_input("Benutzername", key="login_user")
            l_pass = st.text_input("Passwort", type="password", key="login_pass")
            
            if st.button("Login", type="primary", use_container_width=True):
                if AuthManager.login(l_user, l_pass):
                    st.session_state.authenticated = True
                    st.session_state.username = l_user
                    st.success(f"Willkommen {l_user}!")
                    st.rerun()
                else:
                    st.error("Falscher Benutzername oder Passwort.")

            st.divider()
            st.button("Noch nicht angemeldet? Hier registrieren", on_click=show_register, type="secondary", use_container_width=True)

        # --- REGISTER VIEW ---
        else:
            st.subheader("Registrieren")
            r_user = st.text_input("Neuer Benutzername", key="reg_user")
            r_pass = st.text_input("Neues Passwort", type="password", key="reg_pass")
            
            if st.button("Konto erstellen", type="primary", use_container_width=True):
                if r_user and r_pass:
                    if AuthManager.register(r_user, r_pass):
                        st.success("Konto erstellt! Bitte einloggen.")
                        st.session_state.auth_mode = "login"
                        st.rerun()
                    else:
                        st.error("Benutzername vergeben.")
                else:
                    st.warning("Bitte alles ausfüllen.")
            
            st.divider()
            st.button("Bereits ein Konto? Hier einloggen", on_click=show_login, type="secondary", use_container_width=True)

def render_dashboard():
    # Load Data
    render_sidebar()
    username = st.session_state.get("username", "default")
    data = DataManager.load(username)
    
    # CSS for Cards
    st.markdown("""
    <style>
    div[data-testid="stContainer"] {
        background-color: #0E1117;
        border-radius: 10px;
        padding: 10px;
    }
    </style>
    """, unsafe_allow_html=True)
    
    st.title(f"Dashboard")
    st.caption("Verwalte deine Lernprojekte")
    
    st.divider()

    # --- Quick Actions ---
    st.subheader("🚀 Neu Starten")
    c1, c2, c3 = st.columns(3)
    
    with c1:
        with st.container(border=True):
            st.markdown("#### 📁 Neues Projekt")
            st.caption("Beginne ein neues Fach oder Thema.")
            new_name = st.text_input("Projektname", placeholder="z.B. Mathe")
            if st.button("Erstellen ➝", key="create_new_proj", type="primary", use_container_width=True):
                if new_name:
                    DataManager.create_folder(new_name, username)
                    st.rerun()
    
    with c2:
        with st.container(border=True):
            st.markdown("#### 🎤 Audio-Aufnahme")
            st.caption("Lade Vorlesungen hoch oder nimm sie auf.")
            st.button("Starten (Demo)", disabled=True, key="btn_audio", use_container_width=True)

    st.divider()
    
    # --- Projects List ---
    st.subheader("📂 Deine Projekte")
    
    folders = data.get("folders", [])
    if not folders:
        st.info("Noch keine Projekte. Erstelle eins!")
        
    # Grid Layout for Projects
    cols = st.columns(3)
    for i, folder in enumerate(folders):
        col_idx = i % 3
        with cols[col_idx]:
            with st.container(border=True):
                st.markdown(f"**📁 {folder['name']}**")
                st.caption(f"Erstellt: {folder.get('created_at', '?')}")
                if st.button("Öffnen", key=folder['id'], use_container_width=True):
                    # Clean state & Navigate
                    if "text_chunks" in st.session_state: del st.session_state.text_chunks
                    if "vector_index" in st.session_state: del st.session_state.vector_index
                    navigate_to("workspace", folder=folder['id'])

    # Admin Panel (Only for admin_)
    if username == "admin_":
        st.divider()
        with st.expander("👮 Admin Panel", expanded=False):
            users = AuthManager.get_all_users()
            st.metric("Registrierte Nutzer", len(users))
            for u in users:
                if u != "admin_":
                    c_a, c_b, c_c = st.columns([4, 1, 1])
                    c_a.write(f"👤 {u}")
                    # Clear Data Button
                    if c_b.button("🧹", key=f"clear_{u}", help="Nur Daten löschen (Account behalten)"):
                        DataManager.clear_user_data(u)
                        st.toast(f"Daten von {u} gelöscht.")
                        time.sleep(1)
                        st.rerun()
                    # Delete User Button
                    if c_c.button("🗑️", key=f"del_{u}", help="Nutzer komplett löschen"):
                        AuthManager.delete_user(u)
                        st.rerun()

def render_workspace():
    render_sidebar() # Global Sidebar
    
    # Top Navigation Bar
    nav_col1, nav_col2, nav_col3 = st.columns([1, 4, 1])
    
    with nav_col1:
        if st.button("⬅️ Dashboard", type="secondary", use_container_width=True):
            navigate_to("dashboard")
            
    with nav_col2:
        folder_id = st.session_state.current_folder
        folder_name = "Workspace"
        username = st.session_state.get("username", "default")
        
        if folder_id:
            data = DataManager.load(username)
            folder = next((f for f in data["folders"] if f["id"] == folder_id), None)
            if folder: folder_name = folder['name']
            
            # --- Auto-Load Persistent Data ---
            if "plan_data" not in st.session_state:
                saved_plan = DataManager.load_plan(username, folder_id)
                if saved_plan:
                    st.session_state.plan_data = saved_plan
                    
            if "summary_text" not in st.session_state:
                saved_summary = DataManager.load_generated_summary(username, folder_id)
                if saved_summary:
                    st.session_state.summary_text = saved_summary
            
        st.markdown(f"### 📂 {folder_name}")

    with nav_col3:
        pass # Empty now as settings are in sidebar

    st.divider()

    # Main Split Layout
    left_col, right_col = st.columns([1, 2])
    
    with left_col:
        render_file_manager(username, folder_id)

    with right_col:
        # Show tools ONLY if analyzed
        if "text_chunks" in st.session_state:
            _render_tools_tabs(username, folder_id)
        else:
            st.info("👈 Bitte lade PDFs hoch und klicke auf 'Analysieren', um zu starten.")
            st.image("https://placehold.co/600x400?text=Warte+auf+Analyse", use_container_width=True)

@st.dialog("Zusammenfassung")
def show_summary_dialog(title, content):
    st.subheader(title)
    st.markdown(content, unsafe_allow_html=True)

@st.dialog("Vollständige Zusammenfassung", width="large")
def show_full_summary_dialog(anchor, summary_text):
    if not summary_text:
        # Check if we have chunks to generate from
        if "text_chunks" in st.session_state and st.session_state.text_chunks:
             with st.spinner("Erstelle vollständige Zusammenfassung (dies kann einen Moment dauern)..."):
                 summary_text = generate_full_summary(st.session_state.text_chunks)
                 st.session_state.summary_text = summary_text
        else:
            st.error("Keine Dokumente analysiert. Bitte zuerst PDF hochladen und analysieren.")
            return

    if summary_text:
        st.info(f"Gehe zu Abschnitt: {anchor}")
        st.markdown(summary_text, unsafe_allow_html=True)
    else:
        st.error("Konnte Zusammenfassung nicht erstellen.")

@st.dialog("Original Dokument", width="large")
def show_pdf_overlay_dialog(page_num, username, folder_id, filename=None):
    # If filename given, use it. Else naive approach (first pdf).
    if filename:
        path = DataManager.get_pdf_path(filename, username, folder_id)
    else:
        pdfs = DataManager.list_pdfs(username, folder_id)
        path = DataManager.get_pdf_path(pdfs[0], username, folder_id) if pdfs else None
    
    if path:
        with open(path, "rb") as f:
            b64_pdf = base64.b64encode(f.read()).decode('utf-8')
            t = time.time()
            # PDF Iframe with specific page
            pdf_html = f'<iframe src="data:application/pdf;base64,{b64_pdf}#page={page_num}" width="100%" height="800px" type="application/pdf"></iframe>'
            st.markdown(pdf_html, unsafe_allow_html=True)
    else:
        st.error("Kein PDF gefunden.")

def get_coach_prompt(current_day_json, user_instruction):
    """Prompt for the AI Coach to update a specific day."""
    return f"""
    **Rolle**: Du bist ein erfahrener Lern-Coach.
    **Aufgabe**: Bearbeite den folgenden Tagesplan (JSON) basierend auf der Anweisung des Nutzers.
    
    **Aktueller Plan (JSON)**:
    ```json
    {json.dumps(current_day_json, ensure_ascii=False)}
    ```
    
    **Anweisung**: "{user_instruction}"
    
    **Regeln**:
    1. Antworte NUR mit dem aktualisierten JSON. Kein Markdown, kein Text davor/danach.
    2. Behalte die Struktur exakt bei (date, title, objectives, topics, review_focus).
    3. Sei kreativ bei "topics" und "objectives", wenn der Nutzer neue Inhalte will.
    4. Wenn der Nutzer "pausen" oder "weniger" will, passe die Zeiten an.
    """

@st.dialog("🤖 AI Lern-Coach", width="large")
def show_coach_dialog(day_index, day_data):
    st.markdown(f"**Bearbeite Tag {day_index + 1}: {day_data.get('date', 'Datum')}**")
    
    tab_ai, tab_manual = st.tabs(["✨ AI-Assistent", "🛠️ Manuell"])
    
    with tab_ai:
        st.info("Beschreibe, was geändert werden soll. Der AI-Coach passt den Plan an.")
        instruction = st.text_area("Anweisung an den Coach", placeholder="z.B. 'Füge 15 Min Pause ein', 'Mehr Fokus auf Mathe', 'Erkläre das Thema X genauer'")
        
        if st.button("Plan aktualisieren (AI)", key=f"btn_coach_go_{day_index}"):
            if not instruction:
                st.warning("Bitte gib eine Anweisung ein.")
            else:
                with st.spinner("Der Coach arbeitet..."):
                    try:
                        prompt = get_coach_prompt(day_data, instruction)
                        model = genai.GenerativeModel(get_generative_model_name())
                        resp = model.generate_content(prompt)
                        
                        # Parse JSON
                        cleaned = resp.text.replace("```json", "").replace("```", "").strip()
                        new_data = json.loads(cleaned)
                        
                        # Update Session State
                        st.session_state.plan_data[day_index] = new_data
                        
                        # Auto-Save
                        username = st.session_state.get("username", "default")
                        folder_id = st.session_state.current_folder
                        DataManager.save_plan(st.session_state.plan_data, username, folder_id)
                        
                        st.success("Plan aktualisiert!")
                        st.rerun()
                    except Exception as e:
                        st.error(f"Fehler: {e}")
    
    with tab_manual:
        st.warning("Achtung: Bearbeite das JSON-Format vorsichtig!")
        current_json_str = json.dumps(day_data, indent=2, ensure_ascii=False)
        new_json_str = st.text_area("JSON Editor", value=current_json_str, height=400, key=f"json_edit_{day_index}")
        
        if st.button("Speichern (Manuell)", key=f"btn_coach_save_{day_index}"):
            try:
                new_data = json.loads(new_json_str)
                st.session_state.plan_data[day_index] = new_data
                
                # Auto-Save
                username = st.session_state.get("username", "default")
                folder_id = st.session_state.current_folder
                DataManager.save_plan(st.session_state.plan_data, username, folder_id)
                
                st.success("Gespeichert!")
                st.rerun()
            except json.JSONDecodeError as e:
                st.error(f"Ungültiges JSON: {e}")

def _render_tools_tabs(username, folder_id):
    tab1, tab2, tab3, tab4 = st.tabs(["📅 Lernplan", "💬 Dozenten-Chat", "📝 Quiz", "📑 Zusammenfassung"])
    import datetime
    
    # --- TAB 1: Lernplan ---
    with tab1:
        st.header("Smarter Lernplan")
        col1, col2 = st.columns(2)
        with col1:
            start_date = st.date_input("Start-Datum", min_value=datetime.date.today(), value=datetime.date.today())
        with col2:
            exam_date = st.date_input("Prüfungs-Datum", min_value=datetime.date.today() + datetime.timedelta(days=1), value=datetime.date.today() + datetime.timedelta(days=7))
            
        doc_type = st.selectbox("Dokument-Typ", ["Skript / Buch", "Prüfung / Klausur", "Übungsblätter", "Sonstiges"])
        custom_doc_type = ""
        if doc_type == "Sonstiges":
            custom_doc_type = st.text_input("Bitte spezifizieren", placeholder="z.B. Mitschrift")
        
        focus_topic = st.text_input("Fokus-Thema (optional)", placeholder="z.B. Differentialgleichungen")
        
        st.subheader("Lern-Intervall")
        available_days = ["Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag", "Sonntag"]
        selected_days = st.multiselect("Lern-Tage", available_days, default=["Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag"])
        
        # Calculate dates
        study_dates = []
        current = start_date
        while current <= exam_date:
            day_name = available_days[current.weekday()]
            if day_name in selected_days:
                study_dates.append(current)
            current += datetime.timedelta(days=1)
        
        days_count = len(study_dates)
        
        if st.button("Lernplan erstellen (AI)"):
            if days_count == 0:
                st.error("Bitte Wochentage wählen!")
            else:
                st.info(f"Erstelle Plan für {days_count} Sessions...")
                chunks = st.session_state.text_chunks
                # Sampling logic inline
                # Smart Context Sampling (Uniform Distribution instead of Random)
                # We want to cover the WHOLE document to create a complete plan.
                # Flash model has huge context, so we can be generous.
                
                total_chunks = len(chunks)
                if total_chunks <= 30:
                    # Small doc? Take everything.
                    sample = chunks
                else:
                    # Large doc? Take 30 evenly distributed chunks to cover start-to-end
                    step = max(1, total_chunks // 30)
                    sample = chunks[::step]

                context_text = ""
                for c in sample:
                    # Include Page Number clearly for the AI
                    context_text += f"\n=== SOURCE_START [Seite {c.get('page', '?')}] ===\n{c.get('text', '')[:2000]}\n=== SOURCE_END ===\n"
                
                date_list_str = ", ".join([d.strftime('%d.%m.%Y') for d in study_dates])
                
                # Dynamic Prompt based on Doc Type
                doc_context = doc_type
                if doc_type == "Sonstiges": doc_context = custom_doc_type
                
                goal_instruction = "Erstelle einen Lernplan, der den Stoff logisch aufteilt."
                if "Prüfung" in doc_type or "Klausur" in doc_type:
                    goal_instruction = "Dies ist eine PRÜFUNG/KLAUSUR. Analysiere die Aufgabenstellungen und Themen der Prüfung. Der Lernplan soll eine Strategie sein, um genau diese Art von Aufgaben zu lösen. Plane Übungseinheiten für ähnliche Aufgaben ein!"
                elif "Übung" in doc_type:
                    goal_instruction = "Dies sind Übungsblätter. Der Lernplan soll sich auf das aktive Lösen dieser Aufgaben konzentrieren."
                
                # Check for empty focus
                focus_context = ""
                if focus_topic:
                    focus_context = f"Fokus-Thema des Nutzers: {focus_topic}. (Richte den Plan stark danach aus)"
                else:
                    focus_context = "Fokus: Decke den gesamten Inhalt des Dokuments gleichmäßig ab. Erfinde KEINE Themen, die nicht im Text vorkommen."

                # Include Summary Context if available
                summary_context = ""
                if "summary_text" in st.session_state and st.session_state.summary_text:
                    summary_context = f"\n**Referenz-Zusammenfassung (Nutze die IDs hieraus für Links):**\n{st.session_state.summary_text[:10000]}...\n(Gekürzt)"

                prompt = f"""
                **Rolle**: Du bist ein strategischer Lern-Coach.
                Erstelle einen ultimativen Lernplan ("Roadmap to Success").

                **Parameter**:
                - Zeitraum: {days_count} Tage (Exakt!).
                - Termine: {date_list_str}.
                - Ziel: Perfekte Prüfungsvorbereitung.

                **Konzept**:
                1. **Tagessatz**: Jeder Tag hat ein klares Motto (z.B. "Grundlagen", "Deep Dive", "Wiederholung").
                2. **Lernziele**: 3-5 konkrete Ziele pro Tag (Was kann ich am Ende?).
                3. **Inhalte**: Detaillierte Schritte.
                4. **Didaktik**:
                   - Start: Grundlagen.
                   - Mitte: Komplexes & Transfer.
                   - Ende (letzte 20%): Wiederholung & Simulation.
                   - Baue "Spaced Repetition" ein (Wiederhole Thema von Tag 1 an Tag 4).

                **Formatierung & Links**:
                - Verlinke auf die Zusammenfassung mittels `[Siehe Zusammenfassung](#thema-ID)`.
                - Verlinke auf das Original-Dokument: `[📄 S. 12](#page=12)`.
                - Nutze Emojis: 📖 (Lesen), ✍️ (Üben), 🔄 (Wiederholen).

                **Input Text (Auszüge)**:
                {context_text}
                
                {summary_context}

                **Input Metadaten**:
                - Typ: {doc_context}
                - {focus_context}

                **Output Format (Strict JSON)**:
                [
                    {{
                        "date": "DD.MM.YYYY",
                        "day_number": 1,
                        "title": "Motto des Tages",
                        "objectives": ["Ziel 1", "Ziel 2", "Ziel 3"],
                        "topics": [
                            {{
                                "title": "Thema A",
                                "time_minutes": 45,
                                "description": "Lies Abschnitt X. Verstehe Y.",
                                "links": ["[Siehe Zusammenfassung](#thema-1)", "[📄 S. 12](#page=12)"],
                                "rationale": "Wichtig für das Verständnis von Z."
                            }}
                        ],
                        "review_focus": "Was soll wiederholt werden?"
                    }}
                ]
                """
                
                try:
                    model_name = st.session_state.get("model_option", "Automatisch")
                    if model_name == "Automatisch": model_name = get_generative_model_name()
                    
                    with st.spinner("AI arbeitet (Kann 30s dauern)..."):
                        resp = genai.GenerativeModel(model_name).generate_content(prompt)
                        
                        # Robust Parsing
                        content = clean_json_output(resp.text)
                        
                        try:
                            st.session_state.plan_data = json.loads(content)
                        except Exception:
                            # Fallback: AST for single quotes (Python list)
                            try:
                                st.session_state.plan_data = ast.literal_eval(content)
                            except Exception as e:
                                st.error(f"Fehler bei Daten-Verarbeitung: {e}")
                                st.write("Raw Output:", resp.text)
                                st.stop()
                        
                        # --- NEW: Generate Summaries for Topics ---
                        st.info("Generiere Detail-Zusammenfassungen für die Themen...")
                        progress_text = "Thema {} von {}"
                        my_bar = st.progress(0, text=progress_text.format(0, 0))
                        
                        all_topics = []
                        for day in st.session_state.plan_data:
                            if 'topics' in day:
                                for t in day['topics']:
                                    all_topics.append(t['title'])
                        
                        total_tops = len(all_topics)
                        for idx, t_title in enumerate(all_topics):
                            my_bar.progress((idx + 1) / total_tops, text=progress_text.format(idx + 1, total_tops))
                            # Generate
                            if t_title not in st.session_state.generated_summaries:
                                summ = generate_topic_summary(t_title, chunks) # Use all chunks for lookup
                                st.session_state.generated_summaries[t_title] = summ
                        
                        my_bar.empty()
                        
                        # Auto-Save
                        username = st.session_state.get("username", "default")
                        folder_id = st.session_state.current_folder
                        DataManager.save_plan(st.session_state.plan_data, username, folder_id)
                        
                        st.rerun()
                except Exception as e:
                    st.error(f"Fehler: {e}")

        # Render Plan
        if "plan_data" in st.session_state:
            st.subheader(f"Dein Fahrplan")
            
            c1, c2 = st.columns([1, 1])
            with c1:
                 if st.button("➕ Tag hinzufügen"):
                    st.session_state.plan_data.append({"date": "Neu", "topic": "Neu", "details": "..."})
                    st.rerun()
            with c2:
                # PDF Download Button
                pdf_data = create_study_plan_pdf(st.session_state.plan_data)
                if pdf_data:
                    st.download_button(
                        label="📄 Als PDF speichern",
                        data=pdf_data,
                        file_name="Mein_Lernplan.pdf",
                        mime="application/pdf",
                        use_container_width=True
                    )
            
            for i, item in enumerate(st.session_state.plan_data):
                # Fallback for old plan structure vs new structure
                date_str = item.get('date', 'Tag X')
                title = item.get('title', item.get('topic', 'Lerneinheit'))
                
                with st.expander(f"📅 {date_str}: {title}", expanded=(i==0)):
                    # New Structure Rendering
                    if 'objectives' in item:
                        st.markdown("**🎯 Lernziele Heute:**")
                        for obj in item['objectives']:
                            st.caption(f"- {obj}")
                        st.divider()
                        
                        for topic in item.get('topics', []):
                            c1, c2 = st.columns([4, 1])
                            with c1:
                                st.markdown(f"**📖 {topic['title']}** ({topic['time_minutes']} min)")
                                st.markdown(topic['description'])
                                if topic.get('rationale'):
                                    st.info(f"💡 *Warum?* {topic['rationale']}")
                            with c2:
                                # Interactive Summary Button
                                t_title = topic['title']
                                if st.button(f"⚡ Kurze Zusammenfassung", key=f"btn_sum_{i}_{t_title}"):
                                    show_summary_dialog(t_title, st.session_state.generated_summaries.get(t_title, "Keine Zusammenfassung verfügbar."))



                                # Render Links as Buttons/Markdown (Original Links)
                                # We parse the links from the description/rationale or just add generic buttons if links exist
                                # Actually the 'links' list from the plan contains strings like "[...](#...)"
                                # We need to parse them to make them functional buttons
                                
                                links = topic.get('links', [])
                                for link in links:
                                    # Parse: [Label](#target)
                                    match = re.search(r"\[(.*?)\]\(#(.*?)\)", link)
                                    if match:
                                        label = match.group(1)
                                        target = match.group(2)
                                        
                                        # Case 1: PDF Page Link (target starts with "page=")
                                        if "page=" in target:
                                            page_num = target.split("=")[1]
                                            if st.button(f"📄 Original S. {page_num}", key=f"btn_pdf_{i}_{t_title}_{page_num}"):
                                                show_pdf_overlay_dialog(page_num, username, folder_id)
                                        
                                        # Case 2: Summary Link (target starts with "thema-")
                                        elif "thema-" in target:
                                             if st.button(f"📑 Voll-Zusammenfassung ({label})", key=f"btn_full_sum_{i}_{t_title}"):
                                                 show_full_summary_dialog(target, st.session_state.get('summary_text'))
                                    else:
                                        # Fallback for raw text
                                        st.markdown(link, unsafe_allow_html=True)

                            
                            st.divider()
                            
                        if item.get('review_focus'):
                            st.markdown(f"**🔄 Wiederholung:** {item['review_focus']}")
                            
                    else:
                        # Old Structure Fallback
                        st.markdown(item.get('details', ''))
                    
                    # Action Buttons Row
                    col_del, col_coach = st.columns([1, 4])
                    with col_del:
                        if st.button("🗑️ Löschen", key=f"pland_{i}"):
                            st.session_state.plan_data.pop(i)
                            st.rerun()
                    with col_coach:
                         if st.button("🤖 Coach / Bearbeiten", key=f"btn_coach_open_{i}"):
                             show_coach_dialog(i, item)


    # --- TAB 2: Chat ---
    with tab2:
        st.header("Dozenten-Chat")
        if "messages" not in st.session_state: st.session_state.messages = []
        for msg in st.session_state.messages:
            st.chat_message(msg["role"]).markdown(msg["content"])
            
        if prompt := st.chat_input("Frage stellen..."):
            st.session_state.messages.append({"role": "user", "content": prompt})
            st.chat_message("user").markdown(prompt)
            with st.spinner("..."):
                ans, ctx = answer_question(prompt, st.session_state.vector_index, st.session_state.text_chunks)
                st.session_state.messages.append({"role": "assistant", "content": ans})
                st.chat_message("assistant").markdown(ans)

    # --- TAB 3: Quiz ---
    with tab3:
        st.header("Quiz")
        if st.button("Neues Quiz generieren"):
            with st.spinner("..."):
                q = generate_quiz(st.session_state.text_chunks)
                if q: 
                    st.session_state.quiz_data = q
                    st.rerun()
        
        if "quiz_data" in st.session_state:
            for i, q in enumerate(st.session_state.quiz_data):
                st.markdown(f"**{i+1}. {q['question']}**")
                c = st.radio(f"Antwort {i+1}", q['options'], key=f"q_{i}", label_visibility="collapsed")
                if st.button(f"Prüfen", key=f"qk_{i}"):
                    if c == q['answer']: st.success("Richtig!")
                    else: st.error(f"Falsch. {q['answer']}")
                st.divider()

    # --- TAB 4: Summary ---
    with tab4:
        st.header("Zusammenfassung")
        lang = st.selectbox("Sprache", ["Deutsch", "English"], key="sum_lang")
        
        summary_focus = st.text_area("Fokus / Anweisung (optional)", placeholder="z.B. 'Fasse nur Kapitel 3 zusammen' oder 'Fokus auf Definitionen'")
        
        if st.button("Erstellen (AI)"):
             with st.spinner("Analysiere..."):
                 if "text_chunks" in st.session_state and st.session_state.text_chunks:
                     st.session_state.summary_text = generate_full_summary(st.session_state.text_chunks, language=lang, focus=summary_focus)
                     
                     # Auto-Save
                     username = st.session_state.get("username", "default")
                     folder_id = st.session_state.current_folder
                     DataManager.save_generated_summary(st.session_state.summary_text, username, folder_id)
                     
                 else:
                     st.error("Bitte zuerst Dokumente analysieren!")
        
        if st.session_state.get("summary_text"):
            st.markdown(st.session_state.summary_text)
            if st.button("💾 Speichern"):
                # Determine title
                pdfs = DataManager.list_pdfs(username, folder_id)
                title = pdfs[0] if pdfs else "Summary"
                DataManager.save_summary(title, st.session_state.summary_text, username, folder_id)
                st.toast("Gespeichert")
        
        st.divider()
        if "generated_summaries" in st.session_state and st.session_state.generated_summaries:
            st.subheader("📌 Themen-Zusammenfassungen (aus Lernplan)")
            for title, content in st.session_state.generated_summaries.items():
                with st.expander(f"📄 {title}"):
                    st.markdown(content)


# Execute Layout

def main():
    if not AuthManager.check_login():
        render_login_screen()
        return

    if st.session_state.current_page == "dashboard":
        render_dashboard()
    elif st.session_state.current_page == "workspace":
        render_workspace()

if __name__ == "__main__":
    main()
