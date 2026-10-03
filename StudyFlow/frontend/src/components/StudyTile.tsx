"use client";

import React from "react";

export default function StudyTile({
    title,
    meta,
    icon,
    onClick,
    menu,
    className = "",
    previewClassName = "",
}: {
    title: string;
    meta: string;
    icon: React.ReactNode;
    onClick?: () => void;
    menu?: React.ReactNode;
    className?: string;
    previewClassName?: string;
}) {
    return (
        <div
            role={onClick ? "button" : undefined}
            tabIndex={onClick ? 0 : undefined}
            onClick={onClick}
            onKeyDown={(event) => {
                if (!onClick) return;
                if (event.key === "Enter" || event.key === " ") {
                    event.preventDefault();
                    onClick();
                }
            }}
            className={`group relative bg-[#353840] border border-[#45484F] rounded-[10px] overflow-hidden text-left shadow-[0_1px_0_rgba(0,0,0,0.25)] hover:border-[#5B9DFF]/40 hover:bg-[#3A3D45] transition-colors cursor-pointer ${className}`}
        >
            <div className={`h-[86px] px-3.5 flex items-end pb-3 bg-[#2C2F36] ${previewClassName}`}>
                <div className="w-10 h-10 rounded-[8px] bg-[#353840] border border-white/8 flex items-center justify-center text-[#5B9DFF]">
                    {icon}
                </div>
            </div>
            <div className="px-3.5 py-2.5">
                <h3 className="text-[13px] font-semibold text-[#F4F5F7] truncate">{title}</h3>
                <p className="text-[11px] text-[#8B919C] truncate mt-0.5">{meta}</p>
            </div>
            {menu}
        </div>
    );
}
