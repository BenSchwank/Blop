-- Soft-archive for folder file manager. Archived files stay in the folder but
-- are excluded from default AI context and the active file tabs.

alter table public.files
  add column if not exists archived boolean not null default false;

create index if not exists files_folder_archived_idx
  on public.files (folder_id, archived);

comment on column public.files.archived is
  'When true, file is hidden from default KI source selection and the active materials tab.';
