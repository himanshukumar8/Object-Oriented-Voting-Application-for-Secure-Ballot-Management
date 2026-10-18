# ADR-004: Frontend Architecture Selection — React + TypeScript + Vite

**Status:** Accepted  
**Date:** 2026-10-18  
**Context:**  
The legacy application was a Windows console application using `<conio.h>` and `SetConsoleTextAttribute`. To make the system accessible, demonstrable, and cross-platform, a browser-based user interface is required. We evaluated:
1. Server-Side Rendered (SSR) HTML via C++ templates (e.g. inja)
2. React 19 Single Page Application (SPA) with TypeScript and Vite
3. Next.js Fullstack Framework

**Decision:**  
We chose **React 19 + TypeScript + Vite with a Vanilla CSS Design System**.

### Rationale:
1. **Decoupled Client-Server Architecture:** Keeping the frontend as a pure client-side SPA communicating over REST API ensures clean separation of concerns. The C++ backend remains a pure API server that can be tested independently of the UI.
2. **Type Safety & Developer Ergonomics:** TypeScript ensures that request and response models strictly match API schemas, eliminating runtime errors in form handling and vote submission.
3. **Instant Development & Optimized Bundles:** Vite provides sub-second HMR (Hot Module Replacement) during development and tree-shaken static production bundles that can be served directly by the C++ backend or any CDN.
4. **Custom Vanilla CSS Design System:** Using a curated design system with custom CSS variables (dark mode, glassmorphism, fluid typography) produces a stunning, modern look without Tailwind dependency bloat.

### Consequences:
- **Positive:** Rich user experience, interactive ballot review and receipts, responsive on mobile and desktop, zero server-side rendering complexity in C++.
- **Negative:** Requires Node.js build step during frontend development and deployment bundling.
