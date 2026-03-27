import Link from 'next/link';
import { ArrowLeft, Shield } from 'lucide-react';

export const metadata = {
    title: "Datenschutzerklärung – Blop & Blop Study",
    description: "Datenschutzerklärung gemäß DSGVO für die Blop Desktop-App und Blop Study Webanwendung.",
};

export default function DatenschutzPage() {
    return (
        <div className="min-h-screen bg-[#1e1e1e] text-gray-200">
            {/* Header */}
            <div className="sticky top-0 z-10 bg-[#1e1e1e]/95 backdrop-blur border-b border-[#333] px-6 py-4 flex items-center gap-4">
                <Link href="/" className="p-2 hover:bg-[#333] rounded-xl text-gray-400 hover:text-white transition-colors">
                    <ArrowLeft size={20} />
                </Link>
                <div className="flex items-center gap-3">
                    <div className="p-2 rounded-xl bg-green-500/10 text-green-400">
                        <Shield size={20} />
                    </div>
                    <div>
                        <h1 className="text-lg font-semibold text-white">Datenschutzerklärung</h1>
                        <p className="text-xs text-gray-400">Blop &amp; Blop Study · Stand: März 2026</p>
                    </div>
                </div>
            </div>

            {/* Content */}
            <div className="max-w-3xl mx-auto px-6 py-10 space-y-10">

                {/* Intro */}
                <section>
                    <p className="text-gray-300 leading-relaxed">
                        Diese Datenschutzerklärung gilt für die Desktop-Applikation <strong className="text-white">Blop</strong> sowie
                        für die Webanwendung <strong className="text-white">Blop Study</strong> (u. a. unter{' '}
                        <span className="text-[#5E5CE6]">blop-study.com</span> und weiteren bereitgestellten Domains). Wir nehmen den Schutz Ihrer
                        persönlichen Daten sehr ernst und halten uns an die Datenschutz-Grundverordnung (DSGVO) der
                        Europäischen Union.
                    </p>
                </section>

                {/* Block helper */}
                {[
                    {
                        number: "1",
                        title: "Verantwortlicher",
                        content: (
                            <p className="text-gray-300 leading-relaxed">
                                Verantwortlicher im Sinne der DSGVO ist der private Entwickler und Betreiber
                                dieser Anwendung. Da es sich um ein privates, nicht-kommerzielles Projekt handelt, wird
                                die Kontaktadresse bei Anfragen direkt über GitHub (
                                <span className="text-[#5E5CE6]">github.com/BenSchwank</span>) bereitgestellt.
                                Bei datenschutzbezogenen Fragen oder Auskunftsersuchen wenden Sie sich bitte über
                                die dort hinterlegten Kontaktmöglichkeiten.
                            </p>
                        )
                    },
                    {
                        number: "2",
                        title: "Welche Daten werden erhoben?",
                        content: (
                            <div className="space-y-4 text-gray-300">
                                <p className="font-medium text-white">Blop Study (Webanwendung):</p>
                                <ul className="list-disc list-inside space-y-1.5 text-gray-300 ml-2">
                                    <li><strong className="text-gray-200">Accountdaten:</strong> Benutzername und Passwort-Hash; ggf. E-Mail je nach Registrierung; Nutzungskontingente wie Token, XP oder Abo-Stufe, sofern die App diese führt</li>
                                    <li><strong className="text-gray-200">Lerninhalte und Dateien:</strong> Ordner, Metadaten zu Dateien sowie hochgeladene Dateien (z. B. PDFs, Audio) in der Cloud-Speicherung</li>
                                    <li><strong className="text-gray-200">KI-generierte Inhalte:</strong> z. B. Lernpläne, Zusammenfassungen, Karteikarten, Quiz, Wiederholungsbögen, Chatverläufe in der Anwendung</li>
                                    <li><strong className="text-gray-200">Technische Nutzung der KI:</strong> Die Verarbeitung durch Google Gemini erfolgt über einen auf dem Server hinterlegten API-Schlüssel; es ist kein persönlicher Gemini-API-Schlüssel pro Nutzer in der Datenbank vorgesehen</li>
                                    <li><strong className="text-gray-200">Browser:</strong> Benutzername und Sitzungskennung (<code className="text-[#5E5CE6]">session_id</code>) im <code className="text-[#5E5CE6]">localStorage</code> zur Anmeldung</li>
                                </ul>
                                <p className="text-sm text-gray-400 mt-3">
                                    Der Betreiber kann im Rahmen des technischen Betriebs (z. B. Datenbank- und Speicher-Verwaltung) auf gespeicherte Inhalte zugreifen, soweit dies zur Erbringung des Dienstes, zur Fehleranalyse oder zur Erfüllung gesetzlicher Pflichten erforderlich ist.
                                </p>
                                <p className="font-medium text-white mt-4">Blop (Desktop-Applikation):</p>
                                <ul className="list-disc list-inside space-y-1.5 text-gray-300 ml-2">
                                    <li>Die Desktop-App ist eine native Qt-Anwendung, die intern die Blop-Study-Webanwendung anzeigt. Es werden keine zusätzlichen Daten lokal gesammelt.</li>
                                    <li>Es werden keine Analysedaten, Absturz-Logs oder Telemetriedaten erhoben.</li>
                                </ul>
                            </div>
                        )
                    },
                    {
                        number: "3",
                        title: "Zweck der Datenverarbeitung",
                        content: (
                            <ul className="list-disc list-inside space-y-1.5 text-gray-300 ml-2">
                                <li>Bereitstellung der Kernfunktionen (Lernplan-Generierung, Zusammenfassungen, Quiz etc.)</li>
                                <li>Benutzerauthentifizierung und Kontoverwaltung</li>
                                <li>Speicherung Ihrer Lernmaterialien und generierten Inhalte, damit diese sitzungsübergreifend verfügbar bleiben</li>
                                <li>Weiterleitung von Dokumentinhalten an Google Gemini AI zur KI-gestützten Verarbeitung</li>
                            </ul>
                        )
                    },
                    {
                        number: "4",
                        title: "Drittanbieter & Auftragsverarbeiter",
                        content: (
                            <div className="space-y-4 text-gray-300">
                                <div className="bg-[#252526] border border-[#333] rounded-xl p-4 space-y-3">
                                    {[
                                        { name: "Google Gemini AI (Google LLC)", purpose: "KI-gestützte Verarbeitung (Textgenerierung, Analyse von hochgeladenen Dokumenten auf Serverseite)", privacy: "https://policies.google.com/privacy" },
                                        { name: "Supabase Inc.", purpose: "Datenbank (PostgreSQL), Metadaten zu Ordnern/Dateien sowie Dateispeicher (Object Storage) für Uploads", privacy: "https://supabase.com/privacy" },
                                        { name: "Vercel Inc.", purpose: "Hosting der Webanwendung (Frontend)", privacy: "https://vercel.com/legal/privacy-policy" },
                                        { name: "Render Services Inc.", purpose: "Hosting des Backends (API-Server, u. a. Anbindung an Gemini und Supabase)", privacy: "https://render.com/privacy" },
                                        { name: "YouTube / Google (Google LLC)", purpose: "Abruf von Transkripten öffentlicher YouTube-Videos auf Nutzerwunsch", privacy: "https://policies.google.com/privacy" },
                                    ].map((p) => (
                                        <div key={p.name} className="pb-3 border-b border-[#333] last:border-0 last:pb-0">
                                            <p className="font-medium text-white text-sm">{p.name}</p>
                                            <p className="text-xs text-gray-400 mt-0.5">{p.purpose}</p>
                                            <a href={p.privacy} target="_blank" rel="noopener noreferrer" className="text-xs text-[#5E5CE6] hover:underline mt-0.5 block">Datenschutzerklärung →</a>
                                        </div>
                                    ))}
                                </div>
                                <p className="text-sm text-gray-400">
                                    Die Übersendung von Inhalten (z. B. Texte aus Materialien oder hochgeladene Dokumente) an Google Gemini erfolgt über den Server, wenn Sie eine entsprechende Funktion in der App auslösen (z. B. Lernplan, Zusammenfassung, Chat). Die Datenschutzbestimmungen von Google gelten für die dort verarbeiteten Inhalte. Ein Transfer in die USA bzw. zu US-Anbietern kann im Rahmen der EU-Standardvertragsklauseln oder anderer zulässiger Garantien erfolgen.
                                </p>
                            </div>
                        )
                    },
                    {
                        number: "5",
                        title: "Datenspeicherung & Aufbewahrungsdauer",
                        content: (
                            <div className="space-y-2 text-gray-300">
                                <p>Ihre Daten werden in der Datenbank und im Dateispeicher bei <strong className="text-gray-200">Supabase</strong> gespeichert, solange Ihr Account besteht oder bis Sie Löschung verlangen.</p>
                                <p>Hochgeladene Dateien (z. B. PDFs, Audio) bleiben im Speicher bestehen, damit Sie sie in der App weiter nutzen können; sie werden für KI-Funktionen zusätzlich an Gemini übermittelt, wenn Sie eine entsprechende Aktion ausführen.</p>
                                <p>Einträge im <code className="text-[#5E5CE6]">localStorage</code> (Benutzername, <code className="text-[#5E5CE6]">session_id</code>) verbleiben in Ihrem Browser, bis Sie sich abmelden oder den Speicher leeren.</p>
                                <p>Es erfolgt keine automatische Datenlöschung nach einem festen Zeitraum, sofern die App nichts anderes vorsieht. Sie können die Löschung Ihres Accounts und Ihrer Daten verlangen (siehe Abschnitt 7).</p>
                            </div>
                        )
                    },
                    {
                        number: "6",
                        title: "Rechtsgrundlagen",
                        content: (
                            <ul className="list-disc list-inside space-y-1.5 text-gray-300 ml-2">
                                <li><strong className="text-gray-200">Art. 6 Abs. 1 lit. b DSGVO</strong> – Vertragserfüllung: Datenverarbeitung zur Erbringung des Dienstes</li>
                                <li><strong className="text-gray-200">Art. 6 Abs. 1 lit. f DSGVO</strong> – Berechtigte Interessen: Sicherheit, Betrieb und Verbesserung der Anwendung</li>
                                <li><strong className="text-gray-200">Art. 6 Abs. 1 lit. a DSGVO</strong> – Einwilligung: für die Weiterleitung von Inhalten an Drittanbieter (KI-Verarbeitung)</li>
                            </ul>
                        )
                    },
                    {
                        number: "7",
                        title: "Ihre Rechte (Betroffenenrechte)",
                        content: (
                            <ul className="list-disc list-inside space-y-1.5 text-gray-300 ml-2">
                                <li><strong className="text-gray-200">Auskunftsrecht</strong> (Art. 15 DSGVO): Sie können Auskunft über Ihre gespeicherten Daten verlangen.</li>
                                <li><strong className="text-gray-200">Berichtigungsrecht</strong> (Art. 16 DSGVO): Sie können unrichtige Daten korrigieren lassen.</li>
                                <li><strong className="text-gray-200">Löschungsrecht</strong> (Art. 17 DSGVO): Sie können die Löschung Ihrer Daten verlangen.</li>
                                <li><strong className="text-gray-200">Einschränkung der Verarbeitung</strong> (Art. 18 DSGVO)</li>
                                <li><strong className="text-gray-200">Datenübertragbarkeit</strong> (Art. 20 DSGVO)</li>
                                <li><strong className="text-gray-200">Widerspruchsrecht</strong> (Art. 21 DSGVO)</li>
                            </ul>
                        )
                    },
                    {
                        number: "8",
                        title: "Sicherheit",
                        content: (
                            <p className="text-gray-300 leading-relaxed">
                                Passwörter werden gehasht gespeichert. Die Datenübertragung zwischen Browser und Server erfolgt verschlüsselt über HTTPS.
                                Der für Gemini genutzte API-Schlüssel liegt auf dem Server (Hosting bei Render) und wird nicht an Endnutzer als persönlicher Schlüssel ausgegeben.
                                Zugriffe auf die Verwaltungsoberflächen der genannten Anbieter sollten nur über sichere Zugänge erfolgen.
                            </p>
                        )
                    },
                    {
                        number: "9",
                        title: "Cookies & localStorage",
                        content: (
                            <p className="text-gray-300 leading-relaxed">
                                Die Webanwendung verwendet <strong className="text-white">keine Cookies</strong> für Tracking oder Analyse.
                                Es wird ausschließlich der <code className="text-[#5E5CE6]">localStorage</code> des Browsers genutzt,
                                um Ihren Sitzungs-Token und Benutzernamen lokal zu speichern. Diese Daten verlassen Ihren
                                Browser nicht und werden nicht für Werbe- oder Analysezwecke verwendet.
                            </p>
                        )
                    },
                    {
                        number: "10",
                        title: "Änderungen dieser Datenschutzerklärung",
                        content: (
                            <p className="text-gray-300 leading-relaxed">
                                Wir behalten uns vor, diese Datenschutzerklärung bei wesentlichen Änderungen der Funktionalität
                                zu aktualisieren. Das Datum der letzten Änderung ist am Anfang dieser Seite angegeben.
                            </p>
                        )
                    },
                ].map((section) => (
                    <section key={section.number} className="space-y-3">
                        <h2 className="text-lg font-semibold text-white flex items-center gap-2">
                            <span className="w-7 h-7 bg-[#5E5CE6]/20 text-[#5E5CE6] rounded-lg flex items-center justify-center text-sm font-bold shrink-0">
                                {section.number}
                            </span>
                            {section.title}
                        </h2>
                        {section.content}
                    </section>
                ))}

                {/* Footer note */}
                <div className="border-t border-[#333] pt-6 text-center text-xs text-gray-500">
                    <p>Blop &amp; Blop Study · Stand März 2026</p>
                    <Link href="/" className="text-[#5E5CE6] hover:underline mt-1 block">← Zurück zur App</Link>
                </div>
            </div>
        </div>
    );
}
