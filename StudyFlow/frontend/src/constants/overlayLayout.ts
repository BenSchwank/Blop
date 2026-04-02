/**
 * Central Tailwind class fragments for fixed overlays (queue dock, chat, folder KI panel).
 * Queue uses a single right-edge dock so it does not overlap viewer toolbars or the chat FAB.
 */

/** Outer fixed anchor: narrow vertical strip on the right (desktop: vertically centered; mobile: above bottom nav). */
export const OVERLAY_GLOBAL_QUEUE_DOCK =
    "fixed right-0 bottom-24 z-[45] no-print max-md:bottom-32";

/** Inner strip styling (border opens to the left). */
export const OVERLAY_GLOBAL_QUEUE_DOCK_INNER =
    "relative flex flex-col items-center gap-1 rounded-l-xl border border-[#2A2A40] border-r-0 bg-[#0B0B1A]/95 py-2 px-1.5 shadow-xl backdrop-blur-md";

/** Expanded panel opens to the left of the dock (no overlap with top-right browser chrome). */
export const OVERLAY_GLOBAL_QUEUE_PANEL =
    "absolute right-full top-0 mr-2 w-[min(92vw,24rem)] max-h-[min(70vh,28rem)] overflow-y-auto rounded-2xl border border-[#2A2A40] bg-[#0B0B1A]/95 p-3 shadow-2xl backdrop-blur-md custom-scrollbar";

/**
 * Floating chat: inset from right edge so it sits left of the queue dock (~3rem) + gap.
 * max-md: extra lift for MobileNav (ClientLayout uses pb-20 on main).
 */
export const OVERLAY_FLOATING_CHAT =
    "no-print fixed bottom-32 right-20 z-[100] max-md:bottom-36 flex flex-col items-end";

/** Folder page “KI & Verarbeitung” — sm+ inset from right so it clears the global queue dock */
export const OVERLAY_FOLDER_KI_PANEL =
    "no-print fixed bottom-24 right-3 left-3 sm:left-auto sm:right-20 md:bottom-28 z-[85] w-auto sm:w-[min(100vw-1.5rem,20rem)] max-h-[min(55vh,22rem)] flex flex-col rounded-2xl border border-[#2A2A40] bg-[#0B0B1A]/95 shadow-2xl backdrop-blur-md overflow-hidden";
