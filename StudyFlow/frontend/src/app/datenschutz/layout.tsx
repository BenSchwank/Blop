// This layout bypasses the global AuthCheck so the privacy policy
// is accessible to everyone, even without being logged in.
export default function DatenschutzLayout({ children }: { children: React.ReactNode }) {
    return <>{children}</>;
}
