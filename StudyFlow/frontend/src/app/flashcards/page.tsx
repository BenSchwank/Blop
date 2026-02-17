export default function Flashcards() {
    return (
        <div className="p-8 text-white min-h-screen flex flex-col items-center justify-center">
            <div className="w-16 h-16 bg-pink-500/10 rounded-2xl flex items-center justify-center mb-4">
                <span className="text-2xl">🗂️</span>
            </div>
            <h2 className="text-xl font-semibold mb-2">Flashcards</h2>
            <p className="text-zinc-500 max-w-md text-center">
                Lerne effizient mit automatisch generierten Karteikarten.
            </p>
        </div>
    );
}
