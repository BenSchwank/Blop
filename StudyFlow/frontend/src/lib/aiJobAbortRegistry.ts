/**
 * One AbortController per global queue job id (`${folderId}:${jobKey}`).
 * FolderPage registers before fetch; GlobalQueueStatus can abort from anywhere.
 */

const controllers = new Map<string, AbortController>();

export function registerAiJobAbort(jobId: string, controller: AbortController): void {
    const prev = controllers.get(jobId);
    if (prev && prev !== controller) prev.abort();
    controllers.set(jobId, controller);
}

/** Only removes the entry if it still points to this controller (avoids races with a newer job). */
export function unregisterAiJobAbort(jobId: string, controller: AbortController): void {
    if (controllers.get(jobId) !== controller) return;
    controllers.delete(jobId);
}

/** Returns true if a controller was found and abort() was called. */
export function abortAiJob(jobId: string): boolean {
    const c = controllers.get(jobId);
    if (!c) return false;
    c.abort();
    return true;
}

export function isAbortError(e: unknown): boolean {
    if (e instanceof DOMException && e.name === "AbortError") return true;
    if (e instanceof Error && e.name === "AbortError") return true;
    return false;
}
