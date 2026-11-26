export const ceiling = 6000;
export const flightClearance = .65;

export function flightLimits(altitude) {
    return [Math.max(.1, (altitude - 60) * .02), Math.max(250, altitude * 1.6 + 150)];
}

export function fly(eye, yaw, axes, seconds, edge, surfaceAt) {
    const [forward, right, vertical] = axes;
    const magnitude = Math.hypot(forward, right, vertical);
    const speed = 8 + Math.min(352, Math.max(0, eye[1]) * .7);
    const distance = magnitude > 0 ? speed * seconds / magnitude : 0;
    const dx = (forward * Math.sin(yaw) - right * Math.cos(yaw)) * distance;
    const dz = (forward * Math.cos(yaw) + right * Math.sin(yaw)) * distance;
    const bound = value => Math.min(edge, Math.max(-edge, value));
    const x = bound(eye[0] + dx), z = bound(eye[2] + dz);
    const y = Math.min(ceiling, eye[1] + vertical * distance);
    const steps=Math.max(1,Math.ceil(Math.hypot(x-eye[0],z-eye[2])/.6));
    let minimum=surfaceAt(x,z)+flightClearance;
    for(let i=1;i<steps;i++){
        const t=i/steps;
        minimum=Math.max(minimum,surfaceAt(eye[0]+(x-eye[0])*t,eye[2]+(z-eye[2])*t)+flightClearance);
    }
    return [x, Math.max(y,minimum), z];
}
