const fs = require('fs');

// Simple PNG chunk reader or using canvas/jimp if available, or just inspect the file directly
const buf = fs.readFileSync('C:/Users/Tri/.gemini/antigravity/brain/615beb12-d545-47d8-b39d-53c9d81a79e0/.user_uploaded/media_1792102896966_d5a47fb6.png');

console.log('PNG header:', buf.slice(0, 8));
