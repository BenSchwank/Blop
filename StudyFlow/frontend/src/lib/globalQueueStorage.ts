export const BLOP_GLOBAL_QUEUE_KEY = "blop_global_queue";

export type ActiveJob = { id: string; label: string; folderId: string; startedAt: number };
export type RecentJob = {
    id: string;
    label: string;
    folderId: string;
    ok: boolean;
    finishedAt: number;
    errorMessage?: string;
};
export type QueueSnapshot = { updatedAt: number; active: ActiveJob[]; recent: RecentJob[] };

export function readQueueSnapshot(): QueueSnapshot | null {
    if (typeof window === "undefined") return null;
    try {
        const parsed = JSON.parse(localStorage.getItem(BLOP_GLOBAL_QUEUE_KEY) || "null");
        if (parsed && Array.isArray(parsed.active) && Array.isArray(parsed.recent)) {
            return {
                updatedAt: Number(parsed.updatedAt || Date.now()),
                active: parsed.active,
                recent: parsed.recent,
            };
        }
    } catch {
        /* ignore */
    }
    return null;
}

export function writeQueueSnapshot(snapshot: QueueSnapshot): void {
    if (typeof window === "undefined") return;
    localStorage.setItem(
        BLOP_GLOBAL_QUEUE_KEY,
        JSON.stringify({ ...snapshot, updatedAt: Date.now() })
    );
    window.dispatchEvent(new Event("blop_global_queue_updated"));
}

export function reorderActiveJobs(fromIndex: number, toIndex: number): void {
    const snap = readQueueSnapshot();
    if (!snap || snap.active.length < 2) return;
    const n = snap.active.length;
    const from = Math.max(0, Math.min(fromIndex, n - 1));
    const to = Math.max(0, Math.min(toIndex, n - 1));
    if (from === to) return;
    const next = [...snap.active];
    const [item] = next.splice(from, 1);
    next.splice(to, 0, item);
    writeQueueSnapshot({ ...snap, active: next });
}

export function removeActiveJobById(jobId: string): void {
    const snap = readQueueSnapshot();
    if (!snap) return;
    const active = snap.active.filter((j) => j.id !== jobId);
    if (active.length === snap.active.length) return;
    writeQueueSnapshot({ ...snap, active });
}

export function dismissRecentJobById(jobId: string): void {
    const snap = readQueueSnapshot();
    if (!snap) return;
    const recent = snap.recent.filter((j) => j.id !== jobId);
    if (recent.length === snap.recent.length) return;
    writeQueueSnapshot({ ...snap, recent });
}

export const QUEUE_STALE_MS = 25 * 60 * 1000;
export const QUEUE_CANCEL_NOTE =
    "Abgebrochen. Der Server kann die schon gestartete Generierung noch zu Ende schreiben.";

export function formatQueueAge(startedAt: number, now = Date.now()): string {
    const sec = Math.max(0, Math.floor((now - startedAt) / 1000));
    if (sec < 60) return `seit ${sec}s`;
    return `seit ${Math.floor(sec / 60)} Min`;
}

export function expireStaleActiveJobs(maxAgeMs = QUEUE_STALE_MS): void {
    const snap = readQueueSnapshot();
    if (!snap) return;
    const now = Date.now();
    const keep: ActiveJob[] = [];
    const stale: ActiveJob[] = [];
    for (const job of snap.active) {
        if (now - (job.startedAt || 0) > maxAgeMs) stale.push(job);
        else keep.push(job);
    }
    if (stale.length === 0) return;
    const recent: RecentJob[] = [
        ...stale.map((job) => ({
            id: `${job.id}:${now}`,
            label: job.label,
            folderId: job.folderId,
            ok: false,
            finishedAt: now,
            errorMessage: "Abgebrochen (keine Rückmeldung seit 25 Minuten).",
        })),
        ...snap.recent,
    ].slice(0, 15);
    writeQueueSnapshot({ ...snap, active: keep, recent });
}

export function markActiveCancelled(jobId: string): void {
    const snap = readQueueSnapshot();
    if (!snap) return;
    const job = snap.active.find((j) => j.id === jobId);
    if (!job) return;
    const recent: RecentJob[] = [
        {
            id: `${jobId}:${Date.now()}`,
            label: job.label,
            folderId: job.folderId,
            ok: false,
            finishedAt: Date.now(),
            errorMessage: QUEUE_CANCEL_NOTE,
        },
        ...snap.recent,
    ].slice(0, 15);
    writeQueueSnapshot({
        ...snap,
        active: snap.active.filter((j) => j.id !== jobId),
        recent,
    });
}
