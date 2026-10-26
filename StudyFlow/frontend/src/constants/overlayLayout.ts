/**
 * Central Tailwind class fragments for fixed overlays (queue dock, chat, folder KI panel).
 * Queue uses a single right-edge dock so it does not overlap viewer toolbars or the chat FAB.
 * Colors match Blop Notes main-menu chrome (charcoal desk + blue accent).
 */

/** Outer fixed anchor: narrow vertical strip bottom-right (mobile: above bottom nav). */
export const OVERLAY_GLOBAL_QUEUE_DOCK =
    "fixed right-0 bottom-20 z-[45] no-print max-md:bottom-28";

/** Inner strip styling (border opens to the left). */
export const OVERLAY_GLOBAL_QUEUE_DOCK_INNER =
    "relative flex flex-col items-center gap-1 rounded-l-xl border border-[#45484F] border-r-0 bg-[#1A1916]/95 py-2 px-1.5 shadow-xl backdrop-blur-md";

/** Expanded panel: viewport-centered vertically, opens left of the dock strip (sibling in GlobalQueueStatus). */
export const OVERLAY_GLOBAL_QUEUE_PANEL =
    "fixed top-1/2 -translate-y-1/2 right-14 z-[46] w-[min(92vw,24rem)] max-h-[min(70vh,28rem)] overflow-y-auto rounded-xl border border-[#45484F] bg-[#23252A]/95 p-3 shadow-2xl backdrop-blur-md custom-scrollbar";

/**
 * Floating chat FAB anchor: bottom-right, left of the queue dock strip.
 * Mobile: tighter right inset so the FAB stays on-screen above MobileNav.
 */
export const OVERLAY_FLOATING_CHAT =
    "no-print fixed bottom-32 right-20 z-[100] max-md:bottom-36 max-md:right-3 flex flex-col items-end";

/**
 * Open chat panel: fixed right-edge dock on desktop; full-bleed sheet on mobile.
 * Leaves a gap for the queue dock on md+; on max-md uses left/right insets (no right-16).
 */
export const OVERLAY_FLOATING_CHAT_DOCK =
    "no-print fixed z-[100] flex flex-col rounded-2xl border border-[#2A3140] bg-[#12141A] shadow-2xl overflow-hidden top-16 bottom-28 right-16 max-md:inset-x-3 max-md:top-14 max-md:bottom-28 max-md:right-3 max-md:left-3";

/** Folder page “KI & Verarbeitung” — sm+ inset from right so it clears the global queue dock */
export const OVERLAY_FOLDER_KI_PANEL =
    "no-print fixed bottom-24 right-3 left-3 sm:left-auto sm:right-20 md:bottom-28 z-[85] w-auto sm:w-[min(100vw-1.5rem,20rem)] max-h-[min(55vh,22rem)] flex flex-col rounded-xl border border-[#45484F] bg-[#23252A]/95 shadow-2xl backdrop-blur-md overflow-hidden";

/**
 * Selection AI editor: right edge overlay — never follows the mouse/selection caret.
 * Higher z than chat so it wins inside the fullscreen editor.
 */
export const OVERLAY_SELECTION_AI_PANEL =
    "no-print fixed top-20 bottom-4 right-3 z-[130] w-[min(100vw-1.5rem,22rem)] sm:right-4 flex flex-col rounded-2xl border border-[#2A3140] bg-[#12141A]/98 shadow-2xl backdrop-blur-md overflow-hidden";
