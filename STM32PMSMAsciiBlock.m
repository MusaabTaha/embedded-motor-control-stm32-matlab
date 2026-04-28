classdef STM32PMSMAsciiBlock < matlab.System
    % Inputs:
    %   ref_rpm
    %   w_m
    %   i_a
    %   i_b
    %   alpha_e
    %   w_e
    %   U_bat
    %
    % Outputs:
    %   d_a
    %   d_b
    %   d_c

    properties
        Port (1,1) string = "COM4"
        BaudRate (1,1) double = 115200
        Timeout (1,1) double = 1.0
        SampleTime (1,1) double = 0.01
    end

    properties(Access = private)
        sp
        lastDuty (1,3) double = [0.5 0.5 0.5]
    end

    methods(Access = protected)
        function setupImpl(obj)
            obj.sp = serialport(obj.Port, obj.BaudRate, "Timeout", obj.Timeout);
            configureTerminator(obj.sp, "LF");
            flush(obj.sp);

            pause(0.2);
            while obj.sp.NumBytesAvailable > 0
                readline(obj.sp);
            end

            % Reset controller if supported
            try
                writeline(obj.sp, "RESET");
                pause(0.05);
                while obj.sp.NumBytesAvailable > 0
                    readline(obj.sp);
                end
            catch
            end

            obj.lastDuty = [0.5 0.5 0.5];
        end

        function [d_a, d_b, d_c] = stepImpl(obj, ref_rpm, w_m, i_a, i_b, alpha_e, w_e, U_bat)
            cmd = sprintf('%d,%d,%d,%d,%d,%d,%d', ...
                round(double(ref_rpm)), ...
                round(double(w_m) * 1000), ...
                round(double(i_a) * 1000), ...
                round(double(i_b) * 1000), ...
                round(double(mod(alpha_e, 2*pi)) * 1000), ...
                round(double(w_e) * 1000), ...
                round(double(U_bat) * 1000));

            line = "";

            try
                writeline(obj.sp, cmd);
                line = readline(obj.sp);
            catch
                line = "";
            end

            if isstring(line)
                line = char(line);
            end

            if ~isempty(line)
                d = sscanf(line, '%d,%d,%d').';
                if numel(d) == 3
                    obj.lastDuty = max(0.0, min(1.0, double(d) / 1000.0));
                end
            end

            d_a = obj.lastDuty(1);
            d_b = obj.lastDuty(2);
            d_c = obj.lastDuty(3);
        end

        function resetImpl(obj)
            obj.lastDuty = [0.5 0.5 0.5];
            if ~isempty(obj.sp)
                flush(obj.sp);
            end
        end

        function releaseImpl(obj)
            if ~isempty(obj.sp)
                flush(obj.sp);
                obj.sp = [];
            end
        end

        function sts = getSampleTimeImpl(obj)
            sts = createSampleTime(obj, "Type", "Discrete", "SampleTime", obj.SampleTime);
        end

        function n = getNumInputsImpl(~)
            n = 7;
        end

        function n = getNumOutputsImpl(~)
            n = 3;
        end

        function [o1,o2,o3] = getOutputSizeImpl(~)
            o1 = [1 1];
            o2 = [1 1];
            o3 = [1 1];
        end

        function [o1,o2,o3] = getOutputDataTypeImpl(~)
            o1 = "double";
            o2 = "double";
            o3 = "double";
        end

        function [o1,o2,o3] = isOutputComplexImpl(~)
            o1 = false;
            o2 = false;
            o3 = false;
        end

        function [o1,o2,o3] = isOutputFixedSizeImpl(~)
            o1 = true;
            o2 = true;
            o3 = true;
        end
    end
end