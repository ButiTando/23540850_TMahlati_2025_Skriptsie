/* Entry for the standalone replay build. Same App; React comes from a CDN
   script rather than the bundle, and theme.css is inlined into the page. */
import { createRoot } from 'react-dom/client';
import App from './App.jsx';
createRoot(document.getElementById('root')).render(<App />);
